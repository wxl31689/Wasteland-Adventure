[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
function Assert-Foundation([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
    Write-Host "PASS $Message"
}
$required = @(
    'WastelandAdventure.uproject', 'Source\WastelandAdventure\WastelandAdventure.Build.cs',
    'Source\WastelandAdventureEditor.Target.cs', 'Config\DefaultEngine.ini', 'Config\DefaultGame.ini',
    'Content\ThirdPerson\Lvl_ThirdPerson.umap', 'AGENTS.md', 'README.md',
    'Docs\Architecture.md', 'Docs\DevelopmentPlan.md', 'Docs\CodingStandards.md', 'Docs\TestPlan.md'
)
foreach ($relative in $required) {
    Assert-Foundation (Test-Path -LiteralPath (Join-Path $projectRoot $relative) -PathType Leaf) "Exists: $relative"
}
$project = Get-Content -LiteralPath (Join-Path $projectRoot 'WastelandAdventure.uproject') -Raw | ConvertFrom-Json
Assert-Foundation ($project.EngineAssociation -eq '5.8') 'Engine association is 5.8'
Assert-Foundation ([bool]($project.Modules | Where-Object { $_.Name -eq 'WastelandAdventure' -and $_.Type -eq 'Runtime' })) 'Native runtime module exists'
$engineConfig = Get-Content -LiteralPath (Join-Path $projectRoot 'Config\DefaultEngine.ini') -Raw
Assert-Foundation ($engineConfig -match 'GameDefaultMap=/Game/ThirdPerson/Lvl_ThirdPerson') 'Basic Third Person map is default'
Assert-Foundation ($engineConfig -match 'DefaultGraphicsPerformance=(EGraphicsPreset::)?Scalable') 'Scalable preset is configured'
Assert-Foundation ($engineConfig -match '(?m)^r\.RayTracing=False\s*$') 'Hardware ray tracing is disabled'
Assert-Foundation ($engineConfig -match '(?m)^r\.RayTracing\.RayTracingProxies\.ProjectEnabled=False\s*$') 'Ray-tracing proxies are disabled'
$gameConfig = Get-Content -LiteralPath (Join-Path $projectRoot 'Config\DefaultGame.ini') -Raw
Assert-Foundation ($gameConfig -match 'ProjectName=Wasteland Adventure') 'Display name is Wasteland Adventure'
Assert-Foundation (Test-Path -LiteralPath (Join-Path $projectRoot 'Binaries\Win64\UnrealEditor-WastelandAdventure.dll')) 'Editor module binary exists (not a substitute for build logs)'
Write-Host 'FOUNDATION_STRUCTURE_AND_CONFIG_PASS. Runtime gameplay must be tested separately.'
