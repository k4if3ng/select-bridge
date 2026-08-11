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
