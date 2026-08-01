param(
    [int]$TimeLimitSeconds = 120
)

$ErrorActionPreference = 'Stop'
$workspace = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$generated = Join-Path $workspace 'reverse\generated'
$runDirectory = Join-Path $generated 'oracle-run'
$hookSource = Join-Path $workspace 'reverse\oracle\palette_hook.asm'
$hookBinary = Join-Path $generated 'palette_hook.bin'
$paletteExecutable = Join-Path $runDirectory 'SKYPAL.EXE'
$dosbox = Join-Path $workspace '.tools\dosbox-x-2026.07.02\bin\x64\Release SDL2\dosbox-x.exe'
$config = Join-Path $runDirectory 'palette-oracle.conf'
$logFile = Join-Path $runDirectory 'palette-oracle-dosbox.log'
$paletteTrace = Join-Path $runDirectory 'ORAPAL.BIN'

New-Item -ItemType Directory -Force -Path $runDirectory | Out-Null
& nasm -f bin $hookSource -o $hookBinary
if ($LASTEXITCODE -ne 0) { throw 'NASM failed to assemble the DOS palette hook.' }
& python (Join-Path $PSScriptRoot 'build_oracle_exe.py') --palette `
    (Join-Path $workspace 'skyroads.exe') $hookBinary $paletteExecutable
if ($LASTEXITCODE -ne 0) { throw 'Could not build the DOS palette oracle executable.' }

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
if (Test-Path -LiteralPath $paletteTrace) {
    Remove-Item -LiteralPath $paletteTrace -Force
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
skypal.exe
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
    throw "DOS palette oracle exceeded ${TimeLimitSeconds}s."
}
if (!(Test-Path -LiteralPath $paletteTrace) -or
    (Get-Item -LiteralPath $paletteTrace).Length -le 4) {
    throw "DOS palette trace is missing or incomplete: $paletteTrace"
}

$bytes = [System.IO.File]::ReadAllBytes($paletteTrace)
$cursor = 0
$records = 0
while ($cursor -lt $bytes.Length) {
    if ($bytes.Length - $cursor -lt 4) {
        throw "DOS palette trace has a truncated record header at byte $cursor."
    }
    $base = [BitConverter]::ToUInt16($bytes, $cursor)
    $count = [BitConverter]::ToUInt16($bytes, $cursor + 2)
    if ($base + $count -gt 256) {
        throw "DOS palette trace has an invalid DAC range at record $records."
    }
    $cursor += 4 + 3 * $count
    if ($cursor -gt $bytes.Length) {
        throw "DOS palette trace has a truncated payload at record $records."
    }
    ++$records
}
Write-Output "Captured $records executable VGA DAC writes: $paletteTrace"
