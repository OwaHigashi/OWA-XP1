param(
  [string]$Port = 'COM3',
  [string]$OutDir = 'screenshots'
)

$ErrorActionPreference = 'Stop'

function Read-LineFromSerial {
  param([System.IO.Ports.SerialPort]$SerialPort)
  $b = New-Object System.Text.StringBuilder
  while ($true) {
    $byte = $SerialPort.ReadByte()
    if ($byte -lt 0) { throw "EOF" }
    if ($byte -eq 10) { break }
    if ($byte -ne 13) { [void]$b.Append([char]$byte) }
  }
  return $b.ToString()
}

function Wait-ForPrefixLine {
  param([System.IO.Ports.SerialPort]$SerialPort, [string]$Prefix)
  while ($true) {
    $line = Read-LineFromSerial -SerialPort $SerialPort
    if ($line.StartsWith($Prefix)) { return $line }
  }
}

function Save-Ppm {
  param([System.IO.Ports.SerialPort]$SerialPort, [string]$Path)
  $SerialPort.DiscardInBuffer()
  $SerialPort.Write("SCREENSHOT PPM`n")
  $hdr = Wait-ForPrefixLine -SerialPort $SerialPort -Prefix 'OK SCREENSHOT'
  if ($hdr -notmatch 'bytes=(\d+)') { throw "bad header: $hdr" }
  $n = [int]$Matches[1]
  $buf = New-Object byte[] $n
  $off = 0
  while ($off -lt $n) {
    $r = $SerialPort.Read($buf, $off, $n - $off)
    if ($r -le 0) { throw "timeout reading PPM" }
    $off += $r
  }
  [System.IO.File]::WriteAllBytes($Path, $buf)
  [void](Wait-ForPrefixLine -SerialPort $SerialPort -Prefix 'OK SCREENSHOT_DONE')
  Write-Host "wrote $Path ($n bytes)"
}

if (!(Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir | Out-Null }

$sp = New-Object System.IO.Ports.SerialPort $Port,115200,'None',8,'one'
$sp.ReadTimeout = 15000
$sp.WriteTimeout = 3000
$sp.NewLine = "`n"
$sp.Open()
try {
  # Opening the port toggles DTR/RTS which resets the ESP32. Wait long
  # enough for the 4-second splash + setup to finish before issuing any
  # commands.
  Start-Sleep -Seconds 8
  $sp.DiscardInBuffer()

  # Confirm device is alive and in PLAY mode.
  $sp.Write("MODE PLAY`n"); Start-Sleep -Milliseconds 600
  $sp.DiscardInBuffer()
  $sp.Write("STATUS`n")
  $st = Wait-ForPrefixLine -SerialPort $sp -Prefix 'OK STATUS'
  Write-Host "before SMF: $st"

  # Enter SMF Player. First entry triggers SD scan (~1s for ~100 files
  # plus log line per file). Wait generously.
  $sp.Write("BUTTON C`n")
  Start-Sleep -Seconds 5
  $sp.DiscardInBuffer()
  $sp.Write("STATUS`n")
  $st = Wait-ForPrefixLine -SerialPort $sp -Prefix 'OK STATUS'
  Write-Host "after BUTTON C: $st"
  if ($st -notmatch 'mode=SMF_PLAYER') {
    Write-Warning "Did not enter SMF_PLAYER mode!"
  }

  $sp.Write("REDRAW`n")
  Start-Sleep -Milliseconds 800
  $sp.DiscardInBuffer()
  Save-Ppm -SerialPort $sp -Path (Join-Path $OutDir '09-smf-stop.ppm')

  # Start playback (B = play/stop toggle). Let several note events fire.
  $sp.Write("BUTTON B`n")
  Start-Sleep -Seconds 4
  $sp.DiscardInBuffer()
  Save-Ppm -SerialPort $sp -Path (Join-Path $OutDir '10-smf-playing.ppm')

  # Stop playback and exit back to PLAY for the user.
  $sp.Write("BUTTON B`n"); Start-Sleep -Milliseconds 600
  $sp.Write("BUTTON C LONG`n"); Start-Sleep -Milliseconds 800
}
finally {
  $sp.Close()
}
