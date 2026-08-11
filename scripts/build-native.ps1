$ErrorActionPreference = 'Stop'

if (-not (Get-Command uv -ErrorAction SilentlyContinue)) {
  throw 'uv is required to provide Python for node-gyp.'
}

$pythonPath = (uv python find).Trim()
if (-not $pythonPath) {
  throw 'uv did not return a Python interpreter path.'
}

$env:npm_config_python = $pythonPath
pnpm exec node-gyp rebuild --directory native/win32
if ($LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}
