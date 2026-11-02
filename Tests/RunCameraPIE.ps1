[CmdletBinding()]
param(
    [string]$EngineRoot = 'F:\levn\game\Epic\UE_5.8',
    [ValidateSet('Default', 'D3D11')][string]$RHI = 'Default'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $projectRoot 'WastelandAdventure.uproject'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
if (Get-Process -Name UnrealEditor -ErrorAction SilentlyContinue) {
    throw 'Close Unreal Editor safely before starting the isolated camera PIE regression.'
}
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$evidenceRoot = Join-Path $projectRoot "Saved\Verification\Phase2_3\Run-$stamp"
New-Item -ItemType Directory -Path $evidenceRoot -Force | Out-Null
$runLog = Join-Path $evidenceRoot 'CameraPIE.log'
$reportRoot = Join-Path $evidenceRoot 'Report'
$rendererArguments = @()
if ($RHI -eq 'D3D11') { $rendererArguments += '-d3d11' }
& $editor $projectFile '-unattended' '-nosplash' '-nop4' '-nosound' '-NoLiveCoding' `
    @rendererArguments `
    '-ExecCmds=Automation RunTests WastelandAdventure.Character.' `
    '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$reportRoot" "-ABSLOG=$runLog" | Out-Host
$editorExit = $LASTEXITCODE
$reportFile = Join-Path $reportRoot 'index.json'
if (-not (Test-Path -LiteralPath $reportFile)) { throw "No camera PIE report. Editor exit=$editorExit; inspect $runLog" }
$report = Get-Content -LiteralPath $reportFile -Raw | ConvertFrom-Json
$expected = @('CameraPIE', 'DodgePIE', 'SprintPIE')
foreach ($name in $expected) {
    $tests = @($report.tests | Where-Object { $_.fullTestPath -eq "WastelandAdventure.Character.$name" })
    if ($editorExit -ne 0 -or $tests.Count -ne 1 -or $tests[0].state -ne 'Success') {
        throw "$name regression failed. Editor exit=$editorExit; inspect $reportFile and $runLog"
    }
}
Write-Host "CAMERA_PIE_REGRESSION_PASS EDITOR_EXIT_CODE=$editorExit REPORT=$reportFile LOG=$runLog"
