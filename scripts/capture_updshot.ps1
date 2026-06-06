param(
  [string]$Port = 'COM10',
  [Parameter(Mandatory = $true)][string]$Out,
  [string]$Taps = '',          # optional "x1,y1;x2,y2;..." UPDTAP injections before capture
  [int]$BootWait = 7,          # wait after open for the open-reset boot to finish
  [int]$ScanWait = 9           # wait after UPDATEMODE for the Wi-Fi scan + list render
)

# Atomic update-mode screenshot: opens the port ONCE (the open-reset boots the
# device to normal mode), waits for boot, sends UPDATEMODE to enter update mode
# over serial (no physical buttons), optionally injects taps via UPDTAP, then
# captures the screen with SCREENSHOT -- all within the same open session so the
# device is never reset back out of update mode.

$ErrorActionPreference = 'Stop'

function Read-Line($sp) {
  $sb = New-Object System.Text.StringBuilder
  while ($true) {
    $b = $sp.ReadByte()
    if ($b -lt 0) { throw 'EOF' }
    if ($b -eq 10) { break }
    if ($b -ne 13) { [void]$sb.Append([char]$b) }
  }
  return $sb.ToString()
}

$sp = New-Object System.IO.Ports.SerialPort $Port, 115200, 'None', 8, 'one'
$sp.ReadTimeout = 9000
$sp.WriteTimeout = 3000
$sp.Open()
try {
  Write-Host "opened; waiting ${BootWait}s for boot..."
  Start-Sleep -Seconds $BootWait
  $sp.DiscardInBuffer()

  $sp.Write("UPDATEMODE`n")
  Write-Host "sent UPDATEMODE; waiting ${ScanWait}s for Wi-Fi scan + list..."
  Start-Sleep -Seconds $ScanWait

  if ($Taps -ne '') {
    foreach ($t in $Taps.Split(';')) {
      $xy = $t.Split(',')
      $sp.DiscardInBuffer()
      $sp.Write("UPDTAP $($xy[0]) $($xy[1])`n")
      Write-Host "tap $($xy[0]),$($xy[1])"
      Start-Sleep -Milliseconds 1500
    }
  }

  $sp.DiscardInBuffer()
  $sp.Write("SCREENSHOT PPM`n")
  $bc = 0
  $deadline = (Get-Date).AddSeconds(10)
  while ($true) {
    $line = Read-Line $sp
    if ($line.StartsWith('OK ') -and ($line -match 'bytes=(\d+)')) { $bc = [int]$Matches[1]; break }
    if ((Get-Date) -gt $deadline) { throw "no SCREENSHOT header (not on a pollable update screen?)" }
  }
  $buf = New-Object byte[] $bc
  $off = 0
  while ($off -lt $bc) {
    $r = $sp.Read($buf, $off, $bc - $off)
    if ($r -le 0) { throw 'payload timeout' }
    $off += $r
  }
  [System.IO.File]::WriteAllBytes($Out, $buf)
  while ($true) { if ((Read-Line $sp) -eq 'OK SCREENSHOT_DONE') { break } }
  Write-Host "Saved $Out ($bc bytes)"
}
finally {
  $sp.Close()
}
