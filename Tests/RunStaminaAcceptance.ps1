[CmdletBinding()]
param(
    [string]$EngineRoot='F:\levn\game\Epic\UE_5.8',
    [switch]$ManualStart
)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
if(Get-Process -Name UnrealEditor -ErrorAction SilentlyContinue){throw 'Close Unreal Editor safely before stamina acceptance.'}
$evidence=Join-Path $projectRoot ('Saved\Verification\Phase3_2\Run-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
$reportRoot=Join-Path $evidence 'Report'
$runLog=Join-Path $evidence 'StaminaAcceptance.log'
$testArguments=@()
if($ManualStart){Write-Host 'After foreground inspection, stop PIE and enter: Automation RunTests WastelandAdventure.'}
else{$testArguments+='-ExecCmds=Automation RunTests WastelandAdventure.'}
& (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe') (Join-Path $projectRoot 'WastelandAdventure.uproject') `
    '-unattended' '-nosplash' '-nop4' '-nosound' '-NoLiveCoding' @testArguments `
    '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$reportRoot" "-ABSLOG=$runLog" | Out-Host
$editorExit=$LASTEXITCODE
$reportFile=Join-Path $reportRoot 'index.json'
if(-not(Test-Path -LiteralPath $reportFile)){throw "No runtime report. Editor exit=$editorExit; inspect $runLog"}
$report=Get-Content -LiteralPath $reportFile -Raw | ConvertFrom-Json
$expected=@('Character.AnimationPIE','Character.CameraPIE','Character.DodgePIE','Character.Integration30FPS','Character.Integration60FPS','Character.SprintPIE','Health.ActorPIE','Health.ComponentRules','Stamina.ComponentRules','Stamina.Integration30FPS','Stamina.Integration60FPS')
$failures=@()
foreach($name in $expected){
    $items=@($report.tests | Where-Object fullTestPath -eq "WastelandAdventure.$name")
    if($items.Count -ne 1 -or $items[0].state -ne 'Success' -or $items[0].errors -ne 0){$failures+=$name}
}
$pass=$editorExit -eq 0 -and $failures.Count -eq 0 -and $report.failed -eq 0 -and $report.notRun -eq 0 -and $report.inProcess -eq 0
[ordered]@{VerifiedAt=(Get-Date).ToString('o');EditorExitCode=$editorExit;Result=$(if($pass){'PASS'}else{'FAIL'});FailedTests=$failures;Report=$reportFile;Log=$runLog} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $evidence 'RunnerResult.json') -Encoding UTF8
if(-not $pass){throw "Stamina/regression acceptance failed: $($failures -join ', '); Editor exit=$editorExit; inspect $reportFile"}
Write-Host "STAMINA_ACCEPTANCE_PASS EDITOR_EXIT_CODE=$editorExit REPORT=$reportFile"
