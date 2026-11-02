[CmdletBinding()]
param(
    [string]$EngineRoot = 'F:\levn\game\Epic\UE_5.8',
    [ValidateSet('Default','D3D11')][string]$RHI = 'Default',
    [switch]$ConfigureAssets,
    [switch]$RebuildBlendSpace
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (Get-Process -Name UnrealEditor -ErrorAction SilentlyContinue) { throw 'Close Unreal Editor safely before animation PIE regression.' }
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$evidenceRoot = Join-Path $projectRoot "Saved\Verification\Phase2_4\Run-$stamp"
New-Item -ItemType Directory -Path $evidenceRoot -Force | Out-Null
$runLog = Join-Path $evidenceRoot 'AnimationPIE.log'
$reportRoot = Join-Path $evidenceRoot 'Report'
$commands = 'Automation RunTests WastelandAdventure.Character.'
if ($RebuildBlendSpace) { $commands = "WastelandAdventure.Animation.RebuildBlendSpace,$commands" }
if ($ConfigureAssets) {
    $configurationScript = (Join-Path $PSScriptRoot 'ConfigureAnimation.py').Replace('\','/')
    if ($configurationScript.Contains(' ')) { throw 'For one-time asset setup use an installation path without spaces.' }
    $commands = "py $configurationScript,$commands"
}
$rendererArguments = @()
if ($RHI -eq 'D3D11') { $rendererArguments += '-d3d11' }
& (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe') (Join-Path $projectRoot 'WastelandAdventure.uproject') `
    '-unattended' '-nosplash' '-nop4' '-nosound' '-NoLiveCoding' @rendererArguments `
    "-ExecCmds=$commands" '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$reportRoot" "-ABSLOG=$runLog" | Out-Host
$editorExit = $LASTEXITCODE
$reportFile = Join-Path $reportRoot 'index.json'
if (-not (Test-Path -LiteralPath $reportFile)) { throw "No PIE report. Editor exit=$editorExit; inspect $runLog" }
$report = Get-Content -LiteralPath $reportFile -Raw | ConvertFrom-Json
foreach ($name in @('AnimationPIE','CameraPIE','DodgePIE','SprintPIE')) {
    $tests = @($report.tests | Where-Object { $_.fullTestPath -eq "WastelandAdventure.Character.$name" })
    if ($editorExit -ne 0 -or $tests.Count -ne 1 -or $tests[0].state -ne 'Success') { throw "$name failed. Editor exit=$editorExit; inspect $reportFile and $runLog" }
}
Write-Host "ANIMATION_PIE_REGRESSION_PASS EDITOR_EXIT_CODE=$editorExit REPORT=$reportFile LOG=$runLog"
