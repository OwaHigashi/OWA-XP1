param(
  [string]$Port = 'COM10',
  [Parameter(Mandatory = $true)][string]$Out
)

# Capture a SINGLE screenshot of whatever is currently on the Core2 screen.
#
# Unlike capture_screenshots.ps1 (which drives the normal-mode UI with
# MODE/TOUCH/BUTTON serial commands), this script sends ONLY the SCREENSHOT
# command. That makes it usable for the Wi-Fi UPDATE MODE screens, which run in
# an isolated loop where the normal command parser is NOT running and only
# SCREENSHOT is honoured (see updMaybeScreenshot() in the sketch).
#
# Workflow for documenting the updater in the manual:
#   1. Hold the center button (B) during the boot splash to enter update mode.
#   2. Touch your way to the screen you want (Wi-Fi list / keyboard / connect /
#      "No Updater" / "No need to update" / a terminal screen).
#   3. Run:  .\scripts\capture_one.ps1 -Port COM10 -Out screenshots\upd-wifi.ppm
#
# Saves a binary PPM (P6). Convert to PNG with your usual ppm->png step.

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
  Start-Sleep -Milliseconds 300
  $sp.DiscardInBuffer()
  $sp.Write("SCREENSHOT PPM`n")

  # Wait for the "OK SCREENSHOT ... bytes=N" header.
  $header = $null
  while ($true) {
    $line = Read-LineFromSerial -SerialPort $sp
    if ($line.StartsWith('OK ') -and ($line -match 'bytes=(\d+)')) { $header = $line; break }
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
