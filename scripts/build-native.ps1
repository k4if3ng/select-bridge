$ErrorActionPreference = 'Stop'

$pythonCommand = Get-Command python -ErrorAction SilentlyContinue
if (-not $pythonCommand) {
  throw 'Python is required by node-gyp. Add python to PATH and try again.'
}

$env:npm_config_python = $pythonCommand.Source
pnpm exec node-gyp rebuild --directory native/win32
if ($LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}
