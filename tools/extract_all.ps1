param(
    [Parameter()]
    [string]$Source,

    [Parameter()]
    [string]$Output
)

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $Source) {
    $Source = $projectRoot
}
if (-not $Output) {
    $Output = Join-Path $projectRoot "extracted"
}

# Pin the community extractor so a future upstream change cannot silently alter
# our decoded fixtures. The wrapper keeps third-party source in the temp folder;
# only generated assets are written to this workspace.
$revision = "4c5917392b6892792c06336df019152a81cdffab"
$repository = "https://github.com/ammaarreshi/SkyRoads-Codex.git"
$toolRoot = Join-Path $env:TEMP "skyroads-extractor-$revision"
$extractor = Join-Path $toolRoot "tools\skyroads_extract.py"

if (-not (Test-Path -LiteralPath $extractor)) {
    New-Item -ItemType Directory -Force -Path $toolRoot | Out-Null
    git -C $toolRoot init --quiet
    git -C $toolRoot remote add origin $repository
    git -C $toolRoot fetch --quiet --depth 1 origin $revision
    git -C $toolRoot checkout --quiet --detach FETCH_HEAD
}

$sourcePath = (Resolve-Path -LiteralPath $Source).Path
New-Item -ItemType Directory -Force -Path $Output | Out-Null
$outputPath = (Resolve-Path -LiteralPath $Output).Path

python $extractor extract --source $sourcePath --output $outputPath
if ($LASTEXITCODE -ne 0) {
    throw "SkyRoads extraction failed with exit code $LASTEXITCODE"
}

python (Join-Path $PSScriptRoot "postprocess_extracted.py") --root $outputPath
if ($LASTEXITCODE -ne 0) {
    throw "SkyRoads post-processing failed with exit code $LASTEXITCODE"
}

Write-Host "Extracted assets: $outputPath"
Write-Host "Browse catalog:   $(Join-Path $outputPath 'index.html')"
