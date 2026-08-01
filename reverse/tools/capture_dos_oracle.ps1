param(
    [int]$TimeLimitSeconds = 20,
    [switch]$PassthroughState,
    [ValidateRange(1, 1702)]
    [int]$StateLimit = 1702
)

$ErrorActionPreference = 'Stop'
$workspace = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$generated = Join-Path $workspace 'reverse\generated'
$runDirectory = Join-Path $generated 'oracle-run'
$oplCaptureDirectory = Join-Path $runDirectory 'opl-capture'
$hookSource = Join-Path $workspace 'reverse\oracle\trace_hook.asm'
$hookBinary = Join-Path $generated 'trace_hook.bin'
$traceExecutable = Join-Path $runDirectory 'SKYTRACE.EXE'
$dosbox = Join-Path $workspace '.tools\dosbox-x-2026.07.02\bin\x64\Release SDL2\dosbox-x.exe'
$config = Join-Path $runDirectory 'oracle.conf'
$logFile = Join-Path $runDirectory 'oracle-dosbox.log'
$captureOpl = !$PassthroughState -and $StateLimit -eq 1702

New-Item -ItemType Directory -Force -Path $runDirectory | Out-Null
if ($captureOpl) {
    New-Item -ItemType Directory -Force -Path $oplCaptureDirectory | Out-Null
}
$nasmArguments = @('-f', 'bin')
if ($PassthroughState) { $nasmArguments += @('-D', 'PASSTHROUGH_STATE') }
$nasmArguments += @('-D', "TRACE_STATE_LIMIT=$StateLimit")
$nasmArguments += @($hookSource, '-o', $hookBinary)
& nasm @nasmArguments
if ($LASTEXITCODE -ne 0) { throw 'NASM failed to assemble the DOS trace hook.' }
& python (Join-Path $PSScriptRoot 'build_oracle_exe.py') `
    (Join-Path $workspace 'skyroads.exe') $hookBinary $traceExecutable
if ($LASTEXITCODE -ne 0) { throw 'Could not build the DOS oracle executable.' }

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

foreach ($name in @('ORASTATE.BIN', 'ORASFX.BIN', 'ORAOPL.DRO', 'ORAFAULT.BIN')) {
    $target = Join-Path $runDirectory $name
    if (Test-Path -LiteralPath $target) { Remove-Item -LiteralPath $target -Force }
}

$knownOplCaptures = @{}
if ($captureOpl) {
    foreach ($file in Get-ChildItem -LiteralPath $oplCaptureDirectory -Filter '*.dro') {
        $knownOplCaptures[$file.FullName] = $true
    }
}
$programCommand = if ($captureOpl) {
    'dx-capture /O /-D skytrace.exe'
} else {
    'skytrace.exe'
}
$noSound = if ($captureOpl) { 'false' } else { 'true' }

$mountedPath = $runDirectory
@"
[sdl]
output=surface
showmenu=false
waitonerror=false

[dosbox]
machine=svga_s3
memsize=16
captures=$oplCaptureDirectory

[log]
logfile=$logFile

[dos]
log console=quiet

[cpu]
core=dynamic
cycles=max

[mixer]
nosound=$noSound

[midi]
mididevice=none

[sblaster]
sbtype=sb16
oplmode=opl2

[autoexec]
mount c "$mountedPath"
c:
$programCommand
"@ | Set-Content -LiteralPath $config -Encoding Ascii

$arguments = @(
    '-conf', ('"' + $config + '"'), '-fastlaunch', '-exit', '-log-con'
)
$process = Start-Process -FilePath $dosbox -ArgumentList $arguments `
    -WorkingDirectory $runDirectory -WindowStyle Hidden -PassThru

$stateTrace = Join-Path $runDirectory 'ORASTATE.BIN'
$soundTrace = Join-Path $runDirectory 'ORASFX.BIN'
$expectedStateBytes = $StateLimit * 44
$deadline = [DateTime]::UtcNow.AddSeconds($TimeLimitSeconds)
while ([DateTime]::UtcNow -lt $deadline) {
    $process.Refresh()
    if ($process.HasExited) { break }
    if (!$captureOpl -and
        (Test-Path -LiteralPath $stateTrace) -and
        (Get-Item -LiteralPath $stateTrace).Length -eq $expectedStateBytes) {
        break
    }
    Start-Sleep -Milliseconds 100
}
$process.Refresh()
if (!$process.HasExited) {
    Stop-Process -Id $process.Id
    $process.WaitForExit()
}

if ($PassthroughState) {
    $faultTrace = Join-Path $runDirectory 'ORAFAULT.BIN'
    if (Test-Path -LiteralPath $faultTrace) {
        $fault = [System.IO.File]::ReadAllBytes($faultTrace)
        if ($fault.Length -ge 6) {
            $faultIp = [BitConverter]::ToUInt16($fault, 0)
            $faultCs = [BitConverter]::ToUInt16($fault, 2)
            throw ('DOS passthrough probe faulted at {0:X4}:{1:X4}.' -f $faultCs, $faultIp)
        }
    }
    Write-Output 'DOS passthrough probe completed without an invalid-opcode fault.'
    exit 0
}

if (!(Test-Path -LiteralPath $stateTrace) -or
    (Get-Item $stateTrace).Length -ne $expectedStateBytes) {
    $faultTrace = Join-Path $runDirectory 'ORAFAULT.BIN'
    if (Test-Path -LiteralPath $faultTrace) {
        $fault = [System.IO.File]::ReadAllBytes($faultTrace)
        if ($fault.Length -ge 6) {
            $faultIp = [BitConverter]::ToUInt16($fault, 0)
            $faultCs = [BitConverter]::ToUInt16($fault, 2)
            throw ('DOS trace probe faulted at {0:X4}:{1:X4}.' -f $faultCs, $faultIp)
        }
    }
    throw "DOS state trace is missing or incomplete: $stateTrace"
}
if (!(Test-Path -LiteralPath $soundTrace)) {
    throw "DOS sound trace is missing: $soundTrace"
}
if ($captureOpl) {
    $newOplCapture = Get-ChildItem -LiteralPath $oplCaptureDirectory -Filter '*.dro' |
        Where-Object { !$knownOplCaptures.ContainsKey($_.FullName) } |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    if ($null -eq $newOplCapture) {
        throw 'DOS OPL capture is missing.'
    }
    $oplTrace = Join-Path $runDirectory 'ORAOPL.DRO'
    Copy-Item -LiteralPath $newOplCapture.FullName -Destination $oplTrace -Force
    Write-Output "Captured DOS OPL register stream: $oplTrace"
}
Write-Output "Captured DOS oracle: $stateTrace"
Write-Output "Captured DOS sound events: $soundTrace"
