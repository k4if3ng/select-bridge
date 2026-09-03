$ErrorActionPreference = 'Stop'

$pythonCommand = Get-Command python -ErrorAction SilentlyContinue
if (-not $pythonCommand) {
  throw 'Python is required by node-gyp. Add python to PATH and try again.'
}

$nodeArchitecture = node -p "process.arch"
if ($LASTEXITCODE -ne 0) {
  throw 'Unable to determine the Node.js architecture.'
}
if ($nodeArchitecture -ne 'x64' -and $nodeArchitecture -ne 'arm64') {
  throw "Unsupported Windows architecture: $nodeArchitecture. Build with x64 or arm64 Node.js."
}

$env:npm_config_python = $pythonCommand.Source
pnpm exec node-gyp rebuild --directory native/win32
if ($LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}
