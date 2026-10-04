#Requires -Version 5.1
[CmdletBinding()]
param([string]$EngineRoot, [switch]$BuildOnly)
. "$PSScriptRoot\Common.ps1"

try {
    Assert-Windows
    $resolvedEngine = Resolve-Ap5Engine $EngineRoot
    Assert-Ap5BuildTools
    Push-Location $script:ProjectRoot
    try {
        Build-Ap5Editor $resolvedEngine
        Initialize-Ap5Map $resolvedEngine
        if (-not $BuildOnly) {
            # This is standalone uncooked gameplay, NOT a packaged distribution.
            # Piping the GUI executable through Invoke-Ap5Tool waits for its exit.
            Invoke-Ap5Tool -FilePath (Join-Path $resolvedEngine 'Engine\Binaries\Win64\UnrealEditor.exe') `
                -Arguments @($script:ProjectFile, '/Game/Generated/Minimal', '-game', '-windowed', '-ResX=1280', '-ResY=720', '-d3d11', '-stdout', '-FullStdOutLogOutput') `
                -LogName 'run.log'
        }
    }
    finally { Pop-Location }
    exit 0
}
catch {
    Write-Host "BUILD/RUN FAILED: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
