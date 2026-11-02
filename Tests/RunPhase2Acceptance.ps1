[CmdletBinding()]
param(
    [string]$EngineRoot = 'F:\levn\game\Epic\UE_5.8',
    [ValidateSet('Default','D3D11')][string]$RHI = 'Default'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (Get-Process -Name UnrealEditor -ErrorAction SilentlyContinue) { throw 'Close Unreal Editor safely before phase 2 acceptance.' }
$evidence = Join-Path $projectRoot ('Saved\Verification\Phase2_5\Run-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
$reportRoot = Join-Path $evidence 'Report'
$runLog = Join-Path $evidence 'Phase2Acceptance.log'
$rendererArguments = @()
if ($RHI -eq 'D3D11') { $rendererArguments += '-d3d11' }
& (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe') (Join-Path $projectRoot 'WastelandAdventure.uproject') `
    '-unattended' '-nosplash' '-nop4' '-nosound' '-NoLiveCoding' @rendererArguments `
    '-ExecCmds=Automation RunTests WastelandAdventure.Character.' '-TestExit=Automation Test Queue Empty' `
    "-ReportExportPath=$reportRoot" "-ABSLOG=$runLog" | Out-Host
$editorExit = $LASTEXITCODE
$reportFile = Join-Path $reportRoot 'index.json'
if (-not (Test-Path -LiteralPath $reportFile)) { throw "No runtime report. Exit=$editorExit; inspect $runLog" }
$report = Get-Content -LiteralPath $reportFile -Raw | ConvertFrom-Json
foreach ($name in @('AnimationPIE','CameraPIE','DodgePIE','Integration30FPS','Integration60FPS','SprintPIE')) {
    $tests = @($report.tests | Where-Object fullTestPath -eq "WastelandAdventure.Character.$name")
    if ($editorExit -ne 0 -or $tests.Count -ne 1 -or $tests[0].state -ne 'Success') { throw "$name failed. Exit=$editorExit; inspect $reportFile and $runLog" }
}
Write-Host "PHASE2_ACCEPTANCE_PASS EDITOR_EXIT_CODE=$editorExit REPORT=$reportFile LOG=$runLog"
