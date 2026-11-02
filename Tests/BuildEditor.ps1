[CmdletBinding()]
param([string]$EngineRoot = 'F:\levn\game\Epic\UE_5.8')

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $projectRoot 'WastelandAdventure.uproject'
$buildTool = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$logDir = Join-Path $projectRoot 'Saved\Verification'
if (-not (Test-Path -LiteralPath $projectFile)) { throw "Project missing: $projectFile" }
if (-not (Test-Path -LiteralPath $buildTool)) { throw "Build tool missing: $buildTool" }
New-Item -ItemType Directory -Path $logDir -Force | Out-Null
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$logFile = Join-Path $logDir "EditorBuild-$stamp.log"
& $buildTool WastelandAdventureEditor Win64 Development "-Project=$projectFile" -WaitMutex -NoEngineChanges -NoHotReloadFromIDE -MaxParallelActions=1 "-Log=$logFile"
$buildExitCode = $LASTEXITCODE
Write-Host "BUILD_EXIT_CODE=$buildExitCode LOG=$logFile"
exit $buildExitCode
