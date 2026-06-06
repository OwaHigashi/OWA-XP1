param(
  [string]$Port = 'COM10',
  [Parameter(Mandatory = $true)][string]$Out,
  [int]$WaitSec = 50
)

# Capture a single UPDATE-MODE screen.
#
# Why this is special: opening the serial port asserts DTR/RTS, which triggers
# the ESP32 auto-reset -> the device reboots to NORMAL mode and leaves update
# mode. So we must open the port FIRST (accepting that reset), then let the user
# enter update mode by holding B and pressing the physical RST button while the
# port stays open. Sending SCREENSHOT over the already-open port does NOT reset.
#
# Flow:
#   1. This script opens the port (the device resets to normal mode -- expected).
#   2. You have $WaitSec seconds: HOLD the center button (B) and press the RST
#      button on the left edge to reboot INTO update mode, then navigate to the
#      screen you want and LEAVE it there.
#   3. After the wait, the script grabs the screen over the same open port.

$ErrorActionPreference = 'Stop'

function Read-LineFromSerial {
  param([System.IO.Ports.SerialPort]$SerialPort)
  $sb = New-Object System.Text.StringBuilder
  while ($true) {
    $b = $SerialPort.ReadByte()
    if ($b -lt 0) { throw 'Serial read returned EOF' }
    if ($b -eq 10) { break }
    if ($b -ne 13) { [void]$sb.Append([char]$b) }
  }
  return $sb.ToString()
}

$sp = New-Object System.IO.Ports.SerialPort $Port, 115200, 'None', 8, 'one'
$sp.ReadTimeout = 8000
$sp.WriteTimeout = 3000
$sp.NewLine = "`n"
$sp.Open()
try {
  Write-Host "Port open. The device just reset to NORMAL mode (expected)."
  Write-Host ">>> NOW: hold B + press RST to enter update mode, reach the screen, and leave it."
  Write-Host ">>> Capturing in $WaitSec seconds..."
  Start-Sleep -Seconds $WaitSec

  $sp.DiscardInBuffer()
  $sp.Write("SCREENSHOT PPM`n")

  $header = $null
  $deadline = (Get-Date).AddSeconds(10)
  while ($true) {
    $line = Read-LineFromSerial -SerialPort $sp
    if ($line.StartsWith('OK ') -and ($line -match 'bytes=(\d+)')) { $header = $line; break }
    if ((Get-Date) -gt $deadline) { throw "No SCREENSHOT header seen. Are you on an update-mode screen?" }
  }
  $byteCount = [int]$Matches[1]

  $buf = New-Object byte[] $byteCount
  $off = 0
  while ($off -lt $byteCount) {
    $r = $sp.Read($buf, $off, $byteCount - $off)
    if ($r -le 0) { throw 'Timed out while reading screenshot payload' }
    $off += $r
  }
  [System.IO.File]::WriteAllBytes($Out, $buf)

  while ($true) {
    $line = Read-LineFromSerial -SerialPort $sp
    if ($line -eq 'OK SCREENSHOT_DONE') { break }
  }
  Write-Host "Saved $Out ($byteCount bytes)"
}
finally {
  $sp.Close()
}
