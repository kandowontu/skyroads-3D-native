param(
    [string]$GhidraHome,
    [string]$JdkHome
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')).Path
if (-not $GhidraHome) {
    $GhidraHome = Join-Path $repoRoot '.tools\ghidra_12.1.2_PUBLIC'
}
if (-not $JdkHome) {
    $JdkHome = Join-Path $repoRoot '.tools\jdk-21.0.12+8'
}

$headless = Join-Path $GhidraHome 'support\analyzeHeadless.bat'
$java = Join-Path $JdkHome 'bin\java.exe'
if (-not (Test-Path -LiteralPath $headless)) {
    throw "Ghidra headless analyzer not found at $headless"
}
if (-not (Test-Path -LiteralPath $java)) {
    throw "JDK 21 not found at $java"
}

$exe = Join-Path $repoRoot 'skyroads.exe'
$symbols = Join-Path $repoRoot 'reverse\generated\symbols.tsv'
$decompilation = Join-Path $repoRoot 'reverse\generated\ghidra_decompiled.c'
$projectDirectory = Join-Path $repoRoot 'reverse\ghidra-projects'
$projectFile = Join-Path $projectDirectory 'SkyRoads.gpr'
$scriptPath = Join-Path $repoRoot 'reverse\ghidra_scripts'
$log = Join-Path $repoRoot 'reverse\generated\ghidra.log'

& python (Join-Path $PSScriptRoot 'analyze_mz.py') $exe
if ($LASTEXITCODE -ne 0) {
    throw "MZ structural analysis failed with exit code $LASTEXITCODE"
}

New-Item -ItemType Directory -Path $projectDirectory -Force | Out-Null
$env:JAVA_HOME = (Resolve-Path -LiteralPath $JdkHome).Path

$common = @(
    $projectDirectory,
    'SkyRoads',
    '-scriptPath', $scriptPath,
    '-postScript', 'ApplySkyRoadsSymbols.java', $symbols,
    '-postScript', 'ExportSkyRoadsDecompilation.java', $decompilation,
    '-log', $log
)

if (Test-Path -LiteralPath $projectFile) {
    & $headless @common -process 'skyroads.exe'
}
else {
    & $headless @common -import $exe -processor 'x86:LE:16:Real Mode' -max-cpu 4
}
if ($LASTEXITCODE -ne 0) {
    throw "Ghidra analysis failed with exit code $LASTEXITCODE"
}
