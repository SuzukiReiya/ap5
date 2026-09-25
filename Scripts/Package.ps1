#Requires -Version 5.1
[CmdletBinding()]
param([string]$EngineRoot)
. "$PSScriptRoot\Common.ps1"

try {
    Assert-Windows
    $resolvedEngine = Resolve-Ap5Engine $EngineRoot
    Assert-Ap5BuildTools
    Push-Location $script:ProjectRoot
    try {
        Build-Ap5Editor $resolvedEngine
        Initialize-Ap5Map $resolvedEngine
        Invoke-Ap5Tool -FilePath (Join-Path $resolvedEngine 'Engine\Build\BatchFiles\RunUAT.bat') `
            -Arguments @('BuildCookRun', "-project=$script:ProjectFile", '-noP4', '-platform=Win64', '-clientconfig=Shipping', '-build', '-cook', '-map=/Game/Generated/Minimal', '-stage', '-pak', '-archive', '-prereqs', '-utf8output', "-archivedirectory=$(Join-Path $script:ProjectRoot 'Builds')") `
            -LogName 'package.log'
    }
    finally { Pop-Location }
    Write-Host 'Packaged output: Builds\Windows\Ap5.exe (distribute the complete Windows directory).'
    exit 0
}
catch {
    Write-Host "PACKAGE FAILED: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
