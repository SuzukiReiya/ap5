#Requires -Version 5.1
[CmdletBinding()]
param([string]$EngineRoot, [switch]$CheckOnly)
. "$PSScriptRoot\Common.ps1"

try {
    Assert-Windows
    if (-not $CheckOnly) {
        if (-not (Get-Ap5BuildTools)) {
            if (-not (Get-Command winget.exe -ErrorAction SilentlyContinue)) {
                throw 'winget is missing. Install/update App Installer from Microsoft Store, or install dependencies manually as described in README.md.'
            }
            Write-Host 'Installing VS 2022 Build Tools (MSVC 14.38 and Windows SDK 22621). UAC and license prompts may appear.'
            & winget.exe install --id Microsoft.VisualStudio.2022.BuildTools --exact --source winget `
                --override '--wait --passive --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --add Microsoft.VisualStudio.Component.VC.14.38.17.8.x86.x64 --add Microsoft.VisualStudio.Component.Windows11SDK.22621'
            if ($LASTEXITCODE -ne 0) {
                throw 'Build Tools installation did not complete. If already installed, use Visual Studio Installer > Modify to add the components in README.md. Reboot if requested.'
            }
        }
        # Engine downloads require the user's Epic login, license agreement and version selection.
        try { $resolvedEngine = Resolve-Ap5Engine $EngineRoot }
        catch {
            Write-Warning $_.Exception.Message
            $launcher = Join-Path ${env:ProgramFiles(x86)} 'Epic Games\Launcher\Portal\Binaries\Win64\EpicGamesLauncher.exe'
            if (-not (Test-Path $launcher) -and (Get-Command winget.exe -ErrorAction SilentlyContinue)) {
                & winget.exe install --id EpicGames.EpicGamesLauncher --exact --source winget
                if ($LASTEXITCODE -ne 0) { Write-Warning 'Launcher installation failed or is already registered. Open/install it manually.' }
            }
            throw 'Manual step: open Epic Games Launcher > Unreal Engine > Library, sign in/accept terms, install UE 5.6.x, then run Setup.ps1 again with -EngineRoot if needed.'
        }
    }
    else { $resolvedEngine = Resolve-Ap5Engine $EngineRoot }

    Assert-Ap5BuildTools
    if (-not $CheckOnly) {
        New-Item -ItemType Directory -Force -Path $script:LocalDirectory | Out-Null
        @{ EngineRoot = $resolvedEngine } | ConvertTo-Json | Set-Content $script:EngineConfig -Encoding UTF8
    }
    Write-Host "Engine: $resolvedEngine"
    Write-Host 'SETUP_READY. Next: .\Scripts\BuildAndRun.ps1'
    exit 0
}
catch {
    Write-Host "SETUP FAILED: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
