# Precompress static web assets for ESPAsyncWebServer gzip serving.
# Large libs (uPlot): keep only *.gz on LittleFS (plain removed after compress).
# CDN redirect mode does not need local plain copies of those libs.
# To bump a large lib: drop the new uncompressed file in data/js/ or data/css/, re-run this script.
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

function Compress-GzipFile {
  param(
    [Parameter(Mandatory = $true)][string]$Path,
    [switch]$RemoveSource
  )
  if (-not (Test-Path $Path)) {
    Write-Warning "Skip missing: $Path"
    return $null
  }
  $out = "$Path.gz"
  $bytes = [System.IO.File]::ReadAllBytes((Resolve-Path $Path))
  $fs = [System.IO.File]::Create($out)
  try {
    $gz = New-Object System.IO.Compression.GZipStream($fs, [System.IO.Compression.CompressionLevel]::Optimal)
    try {
      $gz.Write($bytes, 0, $bytes.Length)
    } finally {
      $gz.Dispose()
    }
  } finally {
    $fs.Dispose()
  }
  $srcLen = $bytes.Length
  $dstLen = (Get-Item $out).Length
  Write-Host ("{0}  {1} -> {2} bytes ({3:P0})" -f $out, $srcLen, $dstLen, ($dstLen / [math]::Max($srcLen, 1)))
  if ($RemoveSource) {
    Remove-Item $Path -Force
    Write-Host "  removed uncompressed source (LittleFS keeps .gz only)"
  }
  return $out
}

$keepBoth = @(
  "data\css\main.css",
  "data\css\nav.css",
  "data\js\nav.js",
  "data\js\programs.js",
  "data\js\json.js",
  "data\js\chart.js"
)

# Large libs: gzip then delete plain so send()/serveStatic serve Content-Encoding: gzip
$largeLibsOnlyGz = @(
  "data\js\uPlot.iife.min.js",
  "data\css\uPlot.min.css"
)

Write-Host "Compressing assets (keep plain + .gz)..."
foreach ($rel in $keepBoth) {
  Compress-GzipFile -Path (Join-Path $root $rel) | Out-Null
}

Write-Host "Compressing large JS/CSS libs (.gz only on LittleFS)..."
foreach ($rel in $largeLibsOnlyGz) {
  Compress-GzipFile -Path (Join-Path $root $rel) -RemoveSource | Out-Null
}

# Remove obsolete Chart.js / jQuery blobs if still present
$obsolete = @(
  "data\js\jquery-3.7.1.min.js",
  "data\js\jquery-3.7.1.min.js.gz",
  "data\js\Chart.2.9.4.bundle.min.js",
  "data\js\Chart.2.9.4.bundle.min.js.gz",
  "data\js\chartjs-datasource.min.js",
  "data\js\chartjs-datasource.min.js.gz"
)
foreach ($rel in $obsolete) {
  $p = Join-Path $root $rel
  if (Test-Path $p) {
    Remove-Item $p -Force
    Write-Host "Removed obsolete $rel"
  }
}

Write-Host "Done."
