Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$script:ProjectRoot = Split-Path $PSScriptRoot -Parent
$script:ProjectFile = Join-Path $script:ProjectRoot 'Ap5.uproject'
$script:LocalDirectory = Join-Path $script:ProjectRoot '.local'
$script:EngineConfig = Join-Path $script:LocalDirectory 'engine.json'

function Assert-Windows {
    if ($env:OS -ne 'Windows_NT') { throw 'Run these scripts in Windows PowerShell, not WSL/Linux.' }
    if ($PSVersionTable.PSVersion -lt [version]'5.1') { throw 'PowerShell 5.1 or newer is required.' }
}

function Resolve-Ap5Engine {
    param([string]$EngineRoot)
    $candidates = @()
    if ($EngineRoot) { $candidates = @($EngineRoot) }
    elseif ($env:AP5_UE_ROOT) { $candidates = @($env:AP5_UE_ROOT) }
    elseif (Test-Path $script:EngineConfig) {
        $candidates = @((Get-Content $script:EngineConfig -Raw | ConvertFrom-Json).EngineRoot)
    }
    else {
        $registryKey = 'HKLM:\SOFTWARE\EpicGames\Unreal Engine\5.6'
        if (Test-Path $registryKey) {
            $candidates += (Get-ItemProperty $registryKey -Name InstalledDirectory).InstalledDirectory
        }
        $candidates += Join-Path $env:ProgramFiles 'Epic Games\UE_5.6'
    }
    foreach ($candidate in $candidates) {
        $versionFile = Join-Path $candidate 'Engine\Build\Build.version'
        if (-not (Test-Path $versionFile)) { continue }
        $version = Get-Content $versionFile -Raw | ConvertFrom-Json
        if ($version.MajorVersion -ne 5 -or $version.MinorVersion -ne 6) {
            throw "Ap5 targets UE 5.6.x. Selected engine is $($version.MajorVersion).$($version.MinorVersion): $candidate"
        }
        foreach ($relative in @('Engine\Build\BatchFiles\Build.bat',
                'Engine\Binaries\Win64\UnrealEditor.exe',
                'Engine\Binaries\Win64\UnrealEditor-Cmd.exe')) {
            if (-not (Test-Path (Join-Path $candidate $relative))) {
                throw "Incomplete Windows UE installation: missing $relative"
            }
        }
        return (Resolve-Path $candidate).Path
    }
    throw 'UE 5.6.x was not found. Install it in Epic Games Launcher, then rerun Setup.ps1 -EngineRoot "D:\Epic Games\UE_5.6". See README.md.'
}

function Get-Ap5BuildTools {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) { return $null }
    $result = & $vswhere -latest -products '*' -version '[17.8,18.0)' -requires Microsoft.VisualStudio.Component.VC.14.38.17.8.x86.x64 -property installationPath
    if ($LASTEXITCODE -ne 0) { throw 'vswhere failed.' }
    if ($result) { return [string]($result | Select-Object -First 1) }
    return $null
}

function Assert-Ap5BuildTools {
    $installation = Get-Ap5BuildTools
    if (-not $installation) {
        throw 'Install VS 2022 C++ build tools with MSVC v143 v14.38 (VS 17.8), then rerun Setup.ps1. See README.md.'
    }
    $sdkRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10'
    $header = Join-Path $sdkRoot 'Include\10.0.22621.0\um\Windows.h'
    $resourceCompiler = Join-Path $sdkRoot 'bin\10.0.22621.0\x64\rc.exe'
    if (-not (Test-Path $header) -or -not (Test-Path $resourceCompiler)) {
        throw 'Windows SDK 10.0.22621.0 is missing. Add it using Visual Studio Installer, then rerun Setup.ps1.'
    }
    Write-Host "Build tools: $installation"
}

function Invoke-Ap5Tool {
    param([string]$FilePath, [string[]]$Arguments, [string]$LogName)
    if (-not (Test-Path -LiteralPath $FilePath -PathType Leaf)) {
        throw "Tool executable not found: $FilePath"
    }
    $logDirectory = Join-Path $script:LocalDirectory 'logs'
    New-Item -ItemType Directory -Force -Path $logDirectory | Out-Null
    $logPath = Join-Path $logDirectory $LogName
    Write-Host "Running: $FilePath"
    Write-Host "Log: $logPath"
    # Older PowerShell turns native stderr into terminating errors with Stop.
    # Check the process exit code instead. Do not suppress a non-zero exit.
    $oldPreference = $ErrorActionPreference
    $toolExitCode = -1
    try {
        $ErrorActionPreference = 'Continue'
        & $FilePath @Arguments 2>&1 | Tee-Object -FilePath $logPath | Out-Host
        $toolExitCode = $LASTEXITCODE
    }
    finally { $ErrorActionPreference = $oldPreference }
    if ($toolExitCode -ne 0) { throw "Tool failed (exit $toolExitCode). See $logPath" }
}

function Build-Ap5Editor {
    param([string]$EngineRoot)
    Invoke-Ap5Tool -FilePath (Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat') `
        -Arguments @('Ap5Editor', 'Win64', 'Development', "-Project=$script:ProjectFile", '-WaitMutex', '-NoHotReloadFromIDE') `
        -LogName 'build.log'
}

function Initialize-Ap5Map {
    param([string]$EngineRoot)
    $mapFile = Join-Path $script:ProjectRoot 'Content\Generated\Minimal.umap'
    $backgroundFile = Join-Path $script:ProjectRoot 'Content\Generated\M_Background.uasset'
    if ((Test-Path $mapFile) -and (Test-Path $backgroundFile)) { return }
    Invoke-Ap5Tool -FilePath (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') `
        -Arguments @($script:ProjectFile, '-run=pythonscript', "-script=$(Join-Path $PSScriptRoot 'CreateMap.py')", '-unattended', '-nop4', '-nullrhi', '-stdout', '-FullStdOutLogOutput') `
        -LogName 'create-map.log'
    if (-not (Test-Path $mapFile)) { throw 'UE did not generate Minimal.umap. See .local/logs/create-map.log.' }
    if (-not (Test-Path $backgroundFile)) { throw 'UE did not generate M_Background.uasset. See .local/logs/create-map.log.' }
}
