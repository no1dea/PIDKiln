# Build LittleFS + firmware and produce firmware.merged.bin for Wokwi.
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$pio = Get-Command pio -ErrorAction SilentlyContinue
if (-not $pio) {
  $pioExe = Join-Path $env:LOCALAPPDATA "Python\pythoncore-3.14-64\Scripts\pio.exe"
  if (Test-Path $pioExe) { $env:Path = "$(Split-Path $pioExe);$env:Path" }
}

Write-Host "Building LittleFS image..."
pio run -e esp32-wrover -t buildfs
Write-Host "Building firmware + merged flash image..."
pio run -e esp32-wrover

$merged = Join-Path $root ".pio\build\esp32-wrover\firmware.merged.bin"
if (-not (Test-Path $merged)) { throw "Missing $merged" }
$len = (Get-Item $merged).Length
Write-Host "Wokwi image ready: $merged ($len bytes)"
if ($len -lt 0x290000) {
  Write-Warning "Merged image looks too small — LittleFS may be missing. Re-run buildfs then pio run."
}
