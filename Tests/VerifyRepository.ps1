[CmdletBinding()]
param([string]$Git = 'F:\levn\app\Git\cmd\git.exe')

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not (Test-Path -LiteralPath (Join-Path $projectRoot '.git'))) { throw 'Project-local Git repository is missing.' }
$tracked = @(& $Git -C $projectRoot ls-files)
if ($LASTEXITCODE -ne 0) { throw 'git ls-files failed' }
$forbidden = @($tracked | Where-Object { $_ -match '(^|/)(Binaries|Intermediate|Saved|DerivedDataCache|\.vs|\.idea|\.vscode)/|\.(sln|slnx|suo|vcxproj)$' })
if ($forbidden.Count) { throw ('Generated files in index: ' + ($forbidden -join ', ')) }
$assets = @($tracked | Where-Object { $_ -match '\.(uasset|umap)$' })
if (-not $assets.Count) { throw 'No Unreal assets are staged; stage the reviewed foundation before running this test.' }
$lfsLines = @(& $Git -C $projectRoot lfs ls-files --long)
if ($LASTEXITCODE -ne 0) { throw 'git lfs ls-files failed' }
$lfsPaths = @{}
$verifiedObjects = 0
foreach ($line in $lfsLines) {
    if ($line -notmatch '^([0-9a-f]{64}) [*-] (.+)$') { throw "Unrecognized LFS record: $line" }
    $oid = $Matches[1]
    $relative = $Matches[2]
    $workingFile = Join-Path $projectRoot $relative
    $objectFile = Join-Path $projectRoot ('.git\lfs\objects\' + $oid.Substring(0,2) + '\' + $oid.Substring(2,2) + '\' + $oid)
    if (-not (Test-Path -LiteralPath $objectFile)) { throw "Missing LFS object: $relative" }
    if ((Get-FileHash -LiteralPath $workingFile -Algorithm SHA256).Hash.ToLowerInvariant() -ne $oid) { throw "Working file does not match index LFS OID: $relative" }
    if ((Get-FileHash -LiteralPath $objectFile -Algorithm SHA256).Hash.ToLowerInvariant() -ne $oid) { throw "Corrupt LFS object: $relative" }
    $lfsPaths[$relative] = $true
    $verifiedObjects++
}
foreach ($asset in $assets) {
    if (-not $lfsPaths.ContainsKey($asset)) { throw "Unreal asset is not stored as an indexed LFS pointer: $asset" }
}
$sample = 'Content/ThirdPerson/Lvl_ThirdPerson.umap'
$pointer = (& $Git -C $projectRoot show ":$sample") -join "`n"
if ($LASTEXITCODE -ne 0 -or $pointer -notmatch '^version https://git-lfs.github.com/spec/v1\n') { throw 'Representative map index entry is not a real LFS pointer.' }
Write-Host "LFS_HASH_VERIFICATION_PASS unrealAssets=$($assets.Count) verifiedLfsObjects=$verifiedObjects"
# The unmodified official template contains pre-existing whitespace defects.
# Check newly authored foundation files without reformatting vendor baseline code.
& $Git -C $projectRoot diff --cached --check -- .gitattributes .gitignore AGENTS.md README.md Docs Tests
if ($LASTEXITCODE -ne 0) { throw 'Authored foundation file whitespace check failed.' }
Write-Host "REPOSITORY_VERIFICATION_PASS tracked=$($tracked.Count) unrealAssets=$($assets.Count) verifiedLfsObjects=$verifiedObjects"
Write-Host 'Verified index pointers, working binary SHA-256, local LFS object SHA-256, and generated-file exclusions. No commit or push performed.'
