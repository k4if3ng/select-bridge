$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$package = Get-Content (Join-Path $projectRoot 'package.json') -Raw | ConvertFrom-Json
$sourceDirectory = Join-Path $projectRoot 'build/windows/app'
$outputDirectory = Join-Path $projectRoot 'release'
$installerScript = Join-Path $projectRoot 'installer/windows/SelectionForward.iss'

if (-not (Test-Path $sourceDirectory)) {
  throw 'Windows app staging is missing. Run pnpm build:windows first.'
}

$command = Get-Command ISCC.exe -ErrorAction SilentlyContinue
$compilerCandidates = @(
  if ($command) { $command.Source }
  (Join-Path $env:LOCALAPPDATA 'Programs/Inno Setup 6/ISCC.exe')
  'C:/Program Files (x86)/Inno Setup 6/ISCC.exe'
  'D:/Scoop/apps/inno-setup/current/ISCC.exe'
)
$compiler = $compilerCandidates | Where-Object { $_ -and (Test-Path $_) } | Select-Object -First 1

if (-not $compiler) {
  throw 'Inno Setup 6 is required to create the Setup EXE. Install it, then run pnpm build:windows again.'
}

New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
& $compiler "/DAppVersion=$($package.version)" "/DProjectRoot=$projectRoot" "/DSourceDir=$sourceDirectory" "/DOutputDir=$outputDirectory" $installerScript
if ($LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}
