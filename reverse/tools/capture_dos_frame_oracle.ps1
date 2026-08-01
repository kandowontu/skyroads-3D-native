param(
    [int]$TimeLimitSeconds = 180
)

$ErrorActionPreference = 'Stop'
$workspace = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$generated = Join-Path $workspace 'reverse\generated'
$runDirectory = Join-Path $generated 'oracle-run'
$hookSource = Join-Path $workspace 'reverse\oracle\frame_hook.asm'
$hookBinary = Join-Path $generated 'frame_hook.bin'
$frameExecutable = Join-Path $runDirectory 'SKYFRAME.EXE'
$dosbox = Join-Path $workspace '.tools\dosbox-x-2026.07.02\bin\x64\Release SDL2\dosbox-x.exe'
$config = Join-Path $runDirectory 'frame-oracle.conf'
$logFile = Join-Path $runDirectory 'frame-oracle-dosbox.log'

New-Item -ItemType Directory -Force -Path $runDirectory | Out-Null
& nasm -f bin $hookSource -o $hookBinary
if ($LASTEXITCODE -ne 0) { throw 'NASM failed to assemble the DOS frame hook.' }
& python (Join-Path $PSScriptRoot 'build_oracle_exe.py') --frame `
    (Join-Path $workspace 'skyroads.exe') $hookBinary $frameExecutable
if ($LASTEXITCODE -ne 0) { throw 'Could not build the DOS frame oracle executable.' }

$dataFiles = @(
    'anim.lzs', 'cars.lzs', 'dashbrd.lzs', 'demo.rec', 'ful_disp.dat',
    'gomenu.lzs', 'helpmenu.lzs', 'intro.lzs', 'intro.snd', 'mainmenu.lzs',
    'muzax.lzs', 'oxy_disp.dat', 'roads.lzs', 'setmenu.lzs', 'sfx.snd',
    'SKYROADS.CFG', 'speed.dat', 'trekdat.lzs',
    'world0.lzs', 'world1.lzs', 'world2.lzs', 'world3.lzs', 'world4.lzs',
    'world5.lzs', 'world6.lzs', 'world7.lzs', 'world8.lzs', 'world9.lzs'
)
foreach ($name in $dataFiles) {
    Copy-Item -LiteralPath (Join-Path $workspace $name) -Destination $runDirectory -Force
}

foreach ($name in @(
    'ORAFRAME.BIN', 'ORAWORK.BIN', 'ORATREK1.BIN', 'ORATRK1P.BIN',
    'ORAMETA.BIN', 'ORAM221.BIN', 'ORAMSK21.BIN', 'ORACAR43.BIN',
    'ORAXFER.BIN', 'ORATREKS.BIN',
    'ORAFAULT.BIN')) {
    $target = Join-Path $runDirectory $name
    if (Test-Path -LiteralPath $target) { Remove-Item -LiteralPath $target -Force }
}

$mountedPath = $runDirectory
@"
[sdl]
output=surface
showmenu=false
waitonerror=false

[dosbox]
machine=svga_s3
memsize=16

[log]
logfile=$logFile

[dos]
log console=quiet

[cpu]
core=dynamic
cycles=max

[mixer]
nosound=true

[midi]
mididevice=none

[sblaster]
sbtype=sb16
oplmode=none

[autoexec]
mount c "$mountedPath"
c:
skyframe.exe
"@ | Set-Content -LiteralPath $config -Encoding Ascii

$arguments = @('-conf', ('"' + $config + '"'), '-fastlaunch', '-exit', '-log-con')
$process = Start-Process -FilePath $dosbox -ArgumentList $arguments `
    -WorkingDirectory $runDirectory -WindowStyle Hidden -PassThru
$deadline = [DateTime]::UtcNow.AddSeconds($TimeLimitSeconds)
while ([DateTime]::UtcNow -lt $deadline) {
    $process.Refresh()
    if ($process.HasExited) { break }
    Start-Sleep -Milliseconds 100
}
$process.Refresh()
if (!$process.HasExited) {
    Stop-Process -Id $process.Id
    $process.WaitForExit()
    throw "DOS framebuffer oracle exceeded ${TimeLimitSeconds}s."
}

$faultTrace = Join-Path $runDirectory 'ORAFAULT.BIN'
if (Test-Path -LiteralPath $faultTrace) {
    $fault = [System.IO.File]::ReadAllBytes($faultTrace)
    if ($fault.Length -ge 4) {
        $faultIp = [BitConverter]::ToUInt16($fault, 0)
        $faultCs = [BitConverter]::ToUInt16($fault, 2)
        throw ('DOS frame probe faulted at {0:X4}:{1:X4}.' -f $faultCs, $faultIp)
    }
    throw 'DOS frame probe generated an invalid fault record.'
}

$frameTrace = Join-Path $runDirectory 'ORAFRAME.BIN'
$workTrace = Join-Path $runDirectory 'ORAWORK.BIN'
$trekTrace = Join-Path $runDirectory 'ORATREK1.BIN'
$trekPostTrace = Join-Path $runDirectory 'ORATRK1P.BIN'
$metaTrace = Join-Path $runDirectory 'ORAMETA.BIN'
$meta221Trace = Join-Path $runDirectory 'ORAM221.BIN'
$mask221Trace = Join-Path $runDirectory 'ORAMSK21.BIN'
$car221Trace = Join-Path $runDirectory 'ORACAR43.BIN'
$transferTrace = Join-Path $runDirectory 'ORAXFER.BIN'
$trekAllTrace = Join-Path $runDirectory 'ORATREKS.BIN'
$expectedFrames = 1702
$expectedBytes = $expectedFrames * 320 * 200
if (!(Test-Path -LiteralPath $frameTrace) -or
    (Get-Item -LiteralPath $frameTrace).Length -ne $expectedBytes) {
    throw "DOS framebuffer trace is missing or incomplete: $frameTrace"
}
if (!(Test-Path -LiteralPath $workTrace) -or
    (Get-Item -LiteralPath $workTrace).Length -ne $expectedBytes) {
    throw "DOS VGA work-buffer trace is missing or incomplete: $workTrace"
}
if (!(Test-Path -LiteralPath $trekTrace) -or
    (Get-Item -LiteralPath $trekTrace).Length -ne 25775) {
    throw "DOS expanded TREKDAT record trace is missing or incomplete: $trekTrace"
}
if (!(Test-Path -LiteralPath $trekPostTrace) -or
    (Get-Item -LiteralPath $trekPostTrace).Length -ne 25775) {
    throw "DOS post-draw TREKDAT record trace is missing or incomplete: $trekPostTrace"
}
if (!(Test-Path -LiteralPath $trekAllTrace) -or
    (Get-Item -LiteralPath $trekAllTrace).Length -ne 210127) {
    throw "DOS all-record TREKDAT trace is missing or incomplete: $trekAllTrace"
}
if (!(Test-Path -LiteralPath $metaTrace) -or
    (Get-Item -LiteralPath $metaTrace).Length -ne 32) {
    throw "DOS renderer metadata trace is missing or incomplete: $metaTrace"
}
if (!(Test-Path -LiteralPath $meta221Trace) -or
    (Get-Item -LiteralPath $meta221Trace).Length -ne 32) {
    throw "DOS tick-221 renderer metadata is missing or incomplete: $meta221Trace"
}
if (!(Test-Path -LiteralPath $mask221Trace) -or
    (Get-Item -LiteralPath $mask221Trace).Length -ne 957) {
    throw "DOS tick-221 ship mask is missing or incomplete: $mask221Trace"
}
if (!(Test-Path -LiteralPath $car221Trace) -or
    (Get-Item -LiteralPath $car221Trace).Length -ne 720) {
    throw "DOS tick-221 car frame is missing or incomplete: $car221Trace"
}
if (!(Test-Path -LiteralPath $transferTrace) -or
    (Get-Item -LiteralPath $transferTrace).Length -eq 0 -or
    (Get-Item -LiteralPath $transferTrace).Length % 8 -ne 0) {
    throw "DOS road-delta call trace is missing or incomplete: $transferTrace"
}
Write-Output "Captured $expectedFrames DOS VGA framebuffers: $frameTrace"
Write-Output "Captured $expectedFrames DOS VGA work buffers: $workTrace"
Write-Output "Captured expanded DOS TREKDAT record 1: $trekTrace"
Write-Output "Captured post-draw DOS TREKDAT record 1: $trekPostTrace"
Write-Output "Captured post-frame DOS TREKDAT records: $trekAllTrace"
Write-Output "Captured DOS renderer metadata: $metaTrace"
Write-Output "Captured tick-221 DOS renderer metadata: $meta221Trace"
Write-Output "Captured tick-221 DOS ship mask: $mask221Trace"
Write-Output "Captured tick-221 DOS car frame: $car221Trace"
Write-Output "Captured DOS road-delta calls: $transferTrace"
