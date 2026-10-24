param([string]$Clang = 'C:\Program Files\LLVM\bin\clang.exe')
$ErrorActionPreference = 'Stop'
$output = Join-Path $PSScriptRoot 'build'
New-Item -ItemType Directory -Force $output | Out-Null
& $Clang -std=c11 -Wall -Wextra -Werror -I (Join-Path $PSScriptRoot 'stubs') `
    (Join-Path $PSScriptRoot 'test_bridge.c') -o (Join-Path $output 'test_bridge.exe')
if ($LASTEXITCODE -ne 0) { throw 'Host test compilation failed' }
& (Join-Path $output 'test_bridge.exe')
if ($LASTEXITCODE -ne 0) { throw 'Host transport tests failed' }
