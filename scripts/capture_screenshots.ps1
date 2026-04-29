param(
  [string]$Port = 'COM4',
  [string]$OutDir = 'screenshots'
)

$ErrorActionPreference = 'Stop'

function Read-LineFromSerial {
  param(
    [System.IO.Ports.SerialPort]$SerialPort
  )

  $builder = New-Object System.Text.StringBuilder
  while ($true) {
    $byte = $SerialPort.ReadByte()
    if ($byte -lt 0) {
      throw "Serial read returned EOF"
    }
    if ($byte -eq 10) {
      break
    }
    if ($byte -ne 13) {
      [void]$builder.Append([char]$byte)
    }
  }
  return $builder.ToString()
}

function Send-Command {
  param(
    [System.IO.Ports.SerialPort]$SerialPort,
    [string]$Command,
    [int]$DelayMs = 250
  )

  $SerialPort.Write($Command + "`n")
  Start-Sleep -Milliseconds $DelayMs
}

function Wait-ForOkLine {
  param(
    [System.IO.Ports.SerialPort]$SerialPort
  )

  while ($true) {
    $line = Read-LineFromSerial -SerialPort $SerialPort
    if ([string]::IsNullOrWhiteSpace($line)) {
      continue
    }
    if ($line.StartsWith('OK ')) {
      return $line
    }
  }
}

function Save-ScreenshotPpm {
  param(
    [System.IO.Ports.SerialPort]$SerialPort,
    [string]$Path
  )

  $SerialPort.DiscardInBuffer()
  $SerialPort.Write("SCREENSHOT PPM`n")

  $headerLine = Wait-ForOkLine -SerialPort $SerialPort
  if ($headerLine -notmatch 'bytes=(\d+)') {
    throw "Unexpected screenshot header: $headerLine"
  }

  $byteCount = [int]$Matches[1]
  $buffer = New-Object byte[] $byteCount
  $offset = 0
  while ($offset -lt $byteCount) {
    $read = $SerialPort.Read($buffer, $offset, $byteCount - $offset)
    if ($read -le 0) {
      throw "Timed out while reading screenshot payload"
    }
    $offset += $read
  }

  [System.IO.File]::WriteAllBytes($Path, $buffer)

  while ($true) {
    $line = Read-LineFromSerial -SerialPort $SerialPort
    if ([string]::IsNullOrWhiteSpace($line)) {
      continue
    }
    if ($line -eq 'OK SCREENSHOT_DONE') {
      break
    }
  }
}

function Prepare-Screen {
  param(
    [System.IO.Ports.SerialPort]$SerialPort,
    [string[]]$Commands
  )

  foreach ($command in $Commands) {
    Send-Command -SerialPort $SerialPort -Command $command -DelayMs 250
  }
  Send-Command -SerialPort $SerialPort -Command 'REDRAW' -DelayMs 300
}

if (!(Test-Path $OutDir)) {
  New-Item -ItemType Directory -Path $OutDir | Out-Null
}

$screens = @(
  @{ Name = '01-direct.ppm';   Commands = @('MODE DIRECT') },
  @{ Name = '02-key.ppm';      Commands = @('MODE KEY') },
  @{ Name = '03-instant.ppm';  Commands = @('MODE INSTANT') },
  @{ Name = '04-sequence.ppm'; Commands = @('MODE SEQUENCE') },
  @{ Name = '05-filter.ppm';   Commands = @('MODE FILTER') },
  @{ Name = '06-filter-active.ppm'; Commands = @('MODE FILTER', 'TOUCH 250 66') },
  @{ Name = '07-mapper-pg1.ppm'; Commands = @('MODE MAPPER') },
  @{ Name = '08-mapper-pg2.ppm'; Commands = @('MODE MAPPER', 'BUTTON B') }
)

$serialPort = New-Object System.IO.Ports.SerialPort $Port,115200,'None',8,'one'
$serialPort.ReadTimeout = 8000
$serialPort.WriteTimeout = 3000
$serialPort.NewLine = "`n"
$serialPort.Open()

try {
  Start-Sleep -Seconds 2
  $serialPort.DiscardInBuffer()
  $serialPort.DiscardOutBuffer()

  Send-Command -SerialPort $serialPort -Command 'HELP' -DelayMs 300
  [void](Wait-ForOkLine -SerialPort $serialPort)

  foreach ($screen in $screens) {
    Prepare-Screen -SerialPort $serialPort -Commands $screen.Commands
    $targetPath = Join-Path $OutDir $screen.Name
    Save-ScreenshotPpm -SerialPort $serialPort -Path $targetPath
    Start-Sleep -Milliseconds 250
  }

  Send-Command -SerialPort $serialPort -Command 'STATUS' -DelayMs 300
  $statusLine = Wait-ForOkLine -SerialPort $serialPort
  $statusPath = Join-Path $OutDir 'status.txt'
  Set-Content -Path $statusPath -Value $statusLine -Encoding UTF8
}
finally {
  $serialPort.Close()
}
