param(
  [string]$Port = 'COM4',
  [string]$OutDir = 'screenshots'
)
$ErrorActionPreference = 'Stop'

function Read-LineFromSerial {
  param([System.IO.Ports.SerialPort]$SerialPort)
  $builder = New-Object System.Text.StringBuilder
  while ($true) {
    $byte = $SerialPort.ReadByte()
    if ($byte -lt 0) { throw "EOF" }
    if ($byte -eq 10) { break }
    if ($byte -ne 13) { [void]$builder.Append([char]$byte) }
  }
  return $builder.ToString()
}
function Send-Command {
  param([System.IO.Ports.SerialPort]$SerialPort,[string]$Command,[int]$DelayMs = 250)
  $SerialPort.Write($Command + "`n")
  Start-Sleep -Milliseconds $DelayMs
}
function Wait-ForOkLine {
  param([System.IO.Ports.SerialPort]$SerialPort)
  while ($true) {
    $line = Read-LineFromSerial -SerialPort $SerialPort
    if ([string]::IsNullOrWhiteSpace($line)) { continue }
    if ($line.StartsWith('OK ')) { return $line }
  }
}
function Save-ScreenshotPpm {
  param([System.IO.Ports.SerialPort]$SerialPort,[string]$Path)
  $SerialPort.DiscardInBuffer()
  $SerialPort.Write("SCREENSHOT PPM`n")
  $headerLine = Wait-ForOkLine -SerialPort $SerialPort
  if ($headerLine -notmatch 'bytes=(\d+)') { throw "Bad header: $headerLine" }
  $byteCount = [int]$Matches[1]
  $buffer = New-Object byte[] $byteCount
  $offset = 0
  while ($offset -lt $byteCount) {
    $read = $SerialPort.Read($buffer, $offset, $byteCount - $offset)
    if ($read -le 0) { throw "Timed out" }
    $offset += $read
  }
  [System.IO.File]::WriteAllBytes($Path, $buffer)
  while ($true) {
    $line = Read-LineFromSerial -SerialPort $SerialPort
    if ([string]::IsNullOrWhiteSpace($line)) { continue }
    if ($line -eq 'OK SCREENSHOT_DONE') { break }
  }
}
function Prepare-Screen {
  param([System.IO.Ports.SerialPort]$SerialPort,[string[]]$Commands)
  foreach ($c in $Commands) { Send-Command -SerialPort $SerialPort -Command $c -DelayMs 250 }
  Send-Command -SerialPort $SerialPort -Command 'REDRAW' -DelayMs 300
}

$screens = @(
  @{ Name = '06-filter-active.ppm'; Commands = @('MODE FILTER', 'TOUCH 250 56') },
  @{ Name = '07-mapper-pg1.ppm';    Commands = @('MODE MAPPER') },
  @{ Name = '08-mapper-pg2.ppm';    Commands = @('MODE MAPPER', 'BUTTON B') }
)

$serialPort = New-Object System.IO.Ports.SerialPort $Port,115200,'None',8,'one'
$serialPort.ReadTimeout = 30000
$serialPort.WriteTimeout = 5000
$serialPort.NewLine = "`n"
$serialPort.Open()
try {
  Start-Sleep -Seconds 2
  $serialPort.DiscardInBuffer()
  Send-Command -SerialPort $serialPort -Command 'HELP' -DelayMs 300
  [void](Wait-ForOkLine -SerialPort $serialPort)
  foreach ($screen in $screens) {
    Prepare-Screen -SerialPort $serialPort -Commands $screen.Commands
    $targetPath = Join-Path $OutDir $screen.Name
    Save-ScreenshotPpm -SerialPort $serialPort -Path $targetPath
    Write-Host "Saved $targetPath"
    Start-Sleep -Milliseconds 250
  }
} finally {
  $serialPort.Close()
}
