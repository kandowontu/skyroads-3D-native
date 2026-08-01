param(
    [int]$TimeLimitSeconds = 30
)

$ErrorActionPreference = 'Stop'
$workspace = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$generated = Join-Path $workspace 'reverse\generated'
$runDirectory = Join-Path $generated 'oracle-run'
$hookSource = Join-Path $workspace 'reverse\oracle\input_hook.asm'
$hookBinary = Join-Path $generated 'input_hook.bin'
$inputExecutable = Join-Path $runDirectory 'SKYINPUT.EXE'
$dosbox = Join-Path $workspace '.tools\dosbox-x-2026.07.02\bin\x64\Release SDL2\dosbox-x.exe'
$config = Join-Path $runDirectory 'input-oracle.conf'
$logFile = Join-Path $runDirectory 'input-oracle-dosbox.log'

New-Item -ItemType Directory -Force -Path $runDirectory | Out-Null
& nasm -f bin $hookSource -o $hookBinary
if ($LASTEXITCODE -ne 0) { throw 'NASM failed to assemble the DOS input hook.' }
& python (Join-Path $PSScriptRoot 'build_oracle_exe.py') --input `
    (Join-Path $workspace 'skyroads.exe') $hookBinary $inputExecutable
if ($LASTEXITCODE -ne 0) { throw 'Could not build the DOS input oracle executable.' }

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
$outputs = @(
    @{ Name = 'ORAKEY.BIN'; Bytes = 2048 * 6 },
    @{ Name = 'ORAJOY.BIN'; Bytes = 2 * 18 * 18 * 14 },
    @{ Name = 'ORAMOUSE.BIN'; Bytes = 4 * 4 * 4 * 4 * 2 * 22 }
)
foreach ($output in $outputs) {
    $target = Join-Path $runDirectory $output.Name
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
skyinput.exe
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
    throw "DOS input oracle exceeded ${TimeLimitSeconds}s."
}

foreach ($output in $outputs) {
    $target = Join-Path $runDirectory $output.Name
    if (!(Test-Path -LiteralPath $target) -or
        (Get-Item -LiteralPath $target).Length -ne $output.Bytes) {
        throw "DOS input trace is missing or incomplete: $target"
    }
    Write-Output "Captured executable input vectors: $target"
}
