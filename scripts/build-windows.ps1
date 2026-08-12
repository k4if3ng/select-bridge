[CmdletBinding()]
param(
  [switch]$SkipSetup
)

$ErrorActionPreference = 'Stop'

pnpm build
if ($LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}

pnpm build:native
if ($LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}

node scripts/build-windows.mjs
if ($LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}

if (-not $SkipSetup) {
  & (Join-Path $PSScriptRoot 'build-setup.ps1')
  if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
  }
}
