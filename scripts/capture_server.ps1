param(
  [string]$Port = 'COM10',
  [string]$Dir  = 'screenshots\.capsrv'
)

# Persistent screenshot server for UPDATE-MODE captures.
#
# Opening the serial port auto-resets the ESP32 (DTR/RTS), which would kick the
# device out of update mode. This server opens the port ONCE (one reset to
# normal mode is expected) and then KEEPS IT OPEN, so the user can enter update
# mode afterwards via the RST button (NOT by unplugging USB) and the open port
# survives. Captures are then triggered by dropping a file, without any reset.
#
# Protocol (files inside $Dir):
#   ready      -> created when the port is open and the server is looping
#   go         -> create it containing the output .ppm path; server captures it
#   result     -> server writes "OK <path>" or "ERR <msg>" after each capture
#   stop       -> create it to shut the server down

$ErrorActionPreference = 'Stop'
if (-not (Test-Path $Dir)) { New-Item -ItemType Directory $Dir | Out-Null }
Get-ChildItem $Dir -File -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue

function Read-LineFromSerial {
  param([System.IO.Ports.SerialPort]$SerialPort)
  $sb = New-Object System.Text.StringBuilder
  while ($true) {
    $b = $SerialPort.ReadByte()
    if ($b -lt 0) { throw 'EOF' }
    if ($b -eq 10) { break }
    if ($b -ne 13) { [void]$sb.Append([char]$b) }
  }
  return $sb.ToString()
}

function Capture-Screen {
  param([System.IO.Ports.SerialPort]$SerialPort, [string]$OutPath)
  $SerialPort.DiscardInBuffer()
  $SerialPort.Write("SCREENSHOT PPM`n")
  $deadline = (Get-Date).AddSeconds(10)
  $bc = 0
  while ($true) {
    $line = Read-LineFromSerial -SerialPort $SerialPort
    if ($line.StartsWith('OK ') -and ($line -match 'bytes=(\d+)')) { $bc = [int]$Matches[1]; break }
    if ((Get-Date) -gt $deadline) { throw "no SCREENSHOT header (not on an update-mode screen?)" }
  }
  $buf = New-Object byte[] $bc
  $off = 0
  while ($off -lt $bc) {
    $r = $SerialPort.Read($buf, $off, $bc - $off)
    if ($r -le 0) { throw 'payload timeout' }
    $off += $r
  }
  [System.IO.File]::WriteAllBytes($OutPath, $buf)
  while ($true) { if ((Read-LineFromSerial -SerialPort $SerialPort) -eq 'OK SCREENSHOT_DONE') { break } }
  return $bc
}

$sp = New-Object System.IO.Ports.SerialPort $Port, 115200, 'None', 8, 'one'
$sp.ReadTimeout = 8000
$sp.WriteTimeout = 3000
$sp.NewLine = "`n"
$sp.Open()
Set-Content "$Dir\ready" "1"
try {
  while ($true) {
    if (Test-Path "$Dir\stop") { break }
    if (Test-Path "$Dir\reset") {
      Remove-Item "$Dir\reset" -Force
      # esptool-style hard reset into RUN mode over the already-open port:
      # pulse RTS (EN) low->high while DTR (GPIO0) stays de-asserted (normal boot).
      $sp.DtrEnable = $false
      $sp.RtsEnable = $true
      Start-Sleep -Milliseconds 150
      $sp.RtsEnable = $false
      Set-Content "$Dir\result" "RESET done"
    }
    if (Test-Path "$Dir\cmd") {
      $c = (Get-Content "$Dir\cmd" -Raw).Trim()
      Remove-Item "$Dir\cmd" -Force
      $sp.Write($c + "`n")
      Set-Content "$Dir\result" "SENT $c"
    }
    if (Test-Path "$Dir\go") {
      $out = (Get-Content "$Dir\go" -Raw).Trim()
      Remove-Item "$Dir\go" -Force
      try {
        $n = Capture-Screen -SerialPort $sp -OutPath $out
        Set-Content "$Dir\result" "OK $out $n"
      } catch {
        Set-Content "$Dir\result" "ERR $($_.Exception.Message)"
      }
    }
    Start-Sleep -Milliseconds 200
  }
} finally {
  $sp.Close()
  Remove-Item "$Dir\ready" -ErrorAction SilentlyContinue
}
