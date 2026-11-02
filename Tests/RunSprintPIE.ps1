[CmdletBinding()]
param([string]$EngineRoot = 'F:\levn\game\Epic\UE_5.8')

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $projectRoot 'WastelandAdventure.uproject'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
if (Get-Process -Name UnrealEditor -ErrorAction SilentlyContinue) {
    throw 'Close Unreal Editor safely before starting this isolated PIE regression session.'
}
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$evidenceRoot = Join-Path $projectRoot "Saved\Verification\Phase2_1\Run-$stamp"
New-Item -ItemType Directory -Path $evidenceRoot -Force | Out-Null
$runLog = Join-Path $evidenceRoot 'SprintPIE.log'
$reportRoot = Join-Path $evidenceRoot 'Report'
# This runs a real rendered Editor and Play In Editor world, not a NullRHI/static check.
& $editor $projectFile '-unattended' '-nosplash' '-nop4' '-nosound' '-NoLiveCoding' `
    '-ExecCmds=Automation RunTests WastelandAdventure.Character.SprintPIE' `
    '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$reportRoot" "-ABSLOG=$runLog" | Out-Host
$editorExit = $LASTEXITCODE
$reportFile = Join-Path $reportRoot 'index.json'
if (-not (Test-Path -LiteralPath $reportFile)) {
    throw "PIE produced no automation report. Editor exit=$editorExit; inspect $runLog"
}
$report = Get-Content -LiteralPath $reportFile -Raw | ConvertFrom-Json
$sprintTests = @($report.tests | Where-Object { $_.fullTestPath -eq 'WastelandAdventure.Character.SprintPIE' })
if ($editorExit -ne 0 -or $sprintTests.Count -ne 1 -or $sprintTests[0].state -ne 'Success') {
    throw "PIE regression failed. Editor exit=$editorExit; inspect $reportFile and $runLog"
}
Write-Host "SPRINT_PIE_REGRESSION_PASS EDITOR_EXIT_CODE=$editorExit REPORT=$reportFile LOG=$runLog"
