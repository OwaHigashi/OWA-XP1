param(
  [string]$Port = 'COM10',
  [string]$Out  = 'screenshots\11-poweroff.ppm',
  [int]$BootWait = 8
)

# Capture the soft power-off confirmation overlay (B long-press).
# Opens the port once (the open-reset boots the device to normal mode), waits for
# boot, sends "BUTTON B LONG" to enter the Power Off? modal, then SCREENSHOT to
# grab it. The modal's wait loop runs updMaybeScreenshot()/updGetTap(), so the
# SCREENSHOT and the trailing UPDTAP (to tap Cancel) are honoured in-modal.

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

  $sp.Write("BUTTON B LONG`n")
  Write-Host "sent BUTTON B LONG (enters Power Off? modal)"
  Start-Sleep -Milliseconds 1200

  $sp.DiscardInBuffer()
  $sp.Write("SCREENSHOT PPM`n")
  $bc = 0
  $deadline = (Get-Date).AddSeconds(10)
  while ($true) {
    $line = Read-Line $sp
    if ($line.StartsWith('OK ') -and ($line -match 'bytes=(\d+)')) { $bc = [int]$Matches[1]; break }
    if ((Get-Date) -gt $deadline) { throw 'no SCREENSHOT header (not on the modal?)' }
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

  # Dismiss the modal by tapping Cancel (center ~231,182).
  $sp.Write("UPDTAP 231 182`n")
  Write-Host "sent UPDTAP 231 182 (Cancel)"
}
finally {
  $sp.Close()
}
