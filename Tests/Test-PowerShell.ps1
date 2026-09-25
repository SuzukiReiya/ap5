#Requires -Version 5.1
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$failed = $false
foreach ($file in (Get-ChildItem $root -Filter '*.ps1' -Recurse)) {
    $tokens = $null
    $parseErrors = $null
    [System.Management.Automation.Language.Parser]::ParseFile($file.FullName, [ref]$tokens, [ref]$parseErrors) | Out-Null
    if ($parseErrors.Count -gt 0) {
        $failed = $true
        Write-Host "FAIL: $($file.FullName)"
        $parseErrors | Format-List | Out-Host
    }
    else { Write-Host "PASS: $($file.Name)" }
}
if ($failed) { exit 1 }
exit 0
