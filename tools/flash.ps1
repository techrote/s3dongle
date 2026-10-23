param([string]$Port = 'COM13')
$ErrorActionPreference = 'Stop'
$build = Join-Path $PSScriptRoot '..\firmware\build'
if (-not (Test-Path (Join-Path $build 'flasher_args.json'))) { throw 'Run tools/build.ps1 first' }
Push-Location $build
try {
    python -m esptool --chip esp32s3 --port $Port --baud 460800 write-flash '@flash_args'
    if ($LASTEXITCODE -ne 0) { throw 'Flashing failed' }
} finally { Pop-Location }
