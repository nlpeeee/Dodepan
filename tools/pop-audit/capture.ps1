param([string]$Port, [string]$OutputPath)
$ErrorActionPreference = 'Stop'
if (-not $Port) {
    $ports = @(Get-CimInstance Win32_SerialPort | Where-Object { $_.PNPDeviceID -match 'VID_6666&PID_4009' } | ForEach-Object { $_.DeviceID })
    if ($ports.Count -ne 1) { throw 'Specify the diagnostic device port: .\capture.ps1 -Port COM5' }
    $Port = $ports[0]
}
if (-not $OutputPath) { $OutputPath = Join-Path $PSScriptRoot ("../../build-audit/pop-audit/device-" + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '.capture') }
if (Test-Path -LiteralPath $OutputPath) { throw 'Output already exists; choose another name.' }
$serial = [System.IO.Ports.SerialPort]::new($Port,115200,[System.IO.Ports.Parity]::None,8,[System.IO.Ports.StopBits]::One)
$serial.ReadTimeout=10000; $serial.WriteTimeout=3000; $serial.DtrEnable=$true
$serial.NewLine="`n"
try {
    $serial.Open()
    Write-Host 'Start a microphone recording, then reproduce the pop. Keep this window focused.'
    $null=Read-Host 'Press Enter immediately when you hear a pop (captures preceding 1.37 seconds)'
    $serial.DiscardInBuffer();$serial.Write('c')
    $line=$serial.ReadLine()
    if ($line.Length -gt 1024) { throw 'Oversized capture header' }
    $meta=$line | ConvertFrom-Json
    if ($meta.format -ne 'DODEPAN_PCM1' -or $meta.rate -ne 24000 -or $meta.frames -lt 1 -or $meta.frames -gt 32768) { throw 'Invalid capture header' }
    $pcm=[byte[]]::new([int]$meta.frames*2);$offset=0
    while ($offset -lt $pcm.Length) { $offset+=$serial.Read($pcm,$offset,$pcm.Length-$offset) }
    $file=[System.IO.File]::Open($OutputPath,[System.IO.FileMode]::CreateNew)
    try {
        $header=[System.Text.Encoding]::UTF8.GetBytes($line+"`n")
        $file.Write($header,0,$header.Length);$file.Write($pcm,0,$pcm.Length)
    } finally { $file.Dispose() }
} finally { $serial.Dispose() }
& python (Join-Path $PSScriptRoot 'decode_capture.py') $OutputPath
if ($LASTEXITCODE -ne 0) { throw 'Capture integrity check failed; raw file retained for inspection.' }
Write-Host 'Reboot the device before the next playback. Keep the microphone recording and note preset, volume and song position.'
