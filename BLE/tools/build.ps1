param([string]$IdfPath = 'C:\Users\_\.codex\esp-idf-v5.5.5',
      [string]$IdfToolsPath = 'C:\Users\_\.codex\idf-tools-v5.5.5')
$ErrorActionPreference = 'Stop'
$env:IDF_TOOLS_PATH = $IdfToolsPath
. (Join-Path $IdfPath 'export.ps1')
$project = Join-Path $PSScriptRoot '..\firmware'
idf.py -C $project build
if ($LASTEXITCODE -ne 0) { throw 'Firmware build failed' }
