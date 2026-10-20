param([Parameter(Mandatory=$true)][string]$GameData, [Parameter(Mandatory=$true)][string]$Output)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$profile = Join-Path $root 'toolchain.local.json'
if (!(Test-Path -LiteralPath $profile)) { throw 'Configure toolchain.local.json as described in BUILDING.md. The game import is preserved for the next attempt.' }
$config = Get-Content -LiteralPath $profile -Raw | ConvertFrom-Json
& $config.python (Join-Path $PSScriptRoot 'build-local.py') --profile $profile --game-data $GameData --output $Output
if ($LASTEXITCODE -ne 0) { throw 'Local recompilation failed. See logs/first-build.log.' }
