[CmdletBinding()]
param(
    [string]$EngineRoot = 'F:\levn\game\Epic\UE_5.8',
    [ValidateSet('Default','D3D11')][string]$RHI = 'Default',
    [switch]$ManualStart
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (Get-Process -Name UnrealEditor -ErrorAction SilentlyContinue) { throw 'Close Unreal Editor safely before health acceptance.' }
$evidence = Join-Path $projectRoot ('Saved\Verification\Phase3_1\Run-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
$reportRoot = Join-Path $evidence 'Report'
$runLog = Join-Path $evidence 'HealthAcceptance.log'
$rendererArguments = @()
if ($RHI -eq 'D3D11') { $rendererArguments += '-d3d11' }
$testArguments = @()
if ($ManualStart) {
    Write-Host 'After foreground/interactive inspection, stop PIE and enter: Automation RunTests WastelandAdventure.'
} else {
    $testArguments += '-ExecCmds=Automation RunTests WastelandAdventure.'
}
& (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe') (Join-Path $projectRoot 'WastelandAdventure.uproject') `
    '-unattended' '-nosplash' '-nop4' '-nosound' '-NoLiveCoding' @rendererArguments @testArguments `
    '-TestExit=Automation Test Queue Empty' `
    "-ReportExportPath=$reportRoot" "-ABSLOG=$runLog" | Out-Host
$editorExit = $LASTEXITCODE
$reportFile = Join-Path $reportRoot 'index.json'
if (-not (Test-Path -LiteralPath $reportFile)) { throw "No runtime report. Exit=$editorExit; inspect $runLog" }
$report = Get-Content -LiteralPath $reportFile -Raw | ConvertFrom-Json
$expected = @(
    'Character.AnimationPIE','Character.CameraPIE','Character.DodgePIE',
    'Character.Integration30FPS','Character.Integration60FPS','Character.SprintPIE',
    'Health.ComponentRules','Health.ActorPIE'
)
foreach ($name in $expected) {
    $tests = @($report.tests | Where-Object fullTestPath -eq "WastelandAdventure.$name")
    if ($editorExit -ne 0 -or $tests.Count -ne 1 -or $tests[0].state -ne 'Success' -or $tests[0].errors -ne 0) { throw "$name failed. Exit=$editorExit; inspect $reportFile and $runLog" }
}
if ($report.failed -ne 0 -or $report.notRun -ne 0 -or $report.inProcess -ne 0) { throw "Incomplete or failed suite: $reportFile" }
[ordered]@{VerifiedAt=(Get-Date).ToString('o');EditorExitCode=$editorExit;ValidatedTests=$expected;Report=$reportFile;Log=$runLog;Result='PASS'} |
    ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $evidence 'RunnerResult.json') -Encoding UTF8
Write-Host "HEALTH_ACCEPTANCE_PASS EDITOR_EXIT_CODE=$editorExit REPORT=$reportFile LOG=$runLog"
