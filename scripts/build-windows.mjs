import { copyFile, mkdir, readFile, rm, writeFile } from 'node:fs/promises';
import { createRequire } from 'node:module';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

import { build } from 'esbuild';
import { rcedit } from 'rcedit';

const require = createRequire(import.meta.url);
const { inject } = require('postject');

const projectRoot = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const intermediateDirectory = join(projectRoot, 'build', 'windows');
const appDirectory = join(intermediateDirectory, 'app');
const portableRoot = join(intermediateDirectory, 'portable');
const portableDirectory = join(portableRoot, 'SelectBridge');
const releaseDirectory = join(projectRoot, 'release');
const bundledEntry = join(intermediateDirectory, 'main.cjs');
const seaBlob = join(intermediateDirectory, 'sea-prep.blob');
const seaConfig = join(intermediateDirectory, 'sea-config.json');
const executablePath = join(appDirectory, 'SelectBridge.exe');
const iconPath = join(projectRoot, 'resources', 'icon.ico');
const manifestPath = join(projectRoot, 'resources', 'windows.manifest');
const packageJson = JSON.parse(await readFile(join(projectRoot, 'package.json'), 'utf8'));
const version = packageJson.version;
const portableArchive = join(
  releaseDirectory,
  `SelectBridge-${version}-windows-x64-portable.zip`,
);
const nativeAddonPath = join(
  projectRoot,
  'native',
  'win32',
  'build',
  'Release',
  'select_bridge_win32_ui.node',
);
const selectionHookNativePath = join(
  projectRoot,
  'node_modules',
  'selection-hook',
  'prebuilds',
  'win32-x64',
  'selection-hook.node',
);
if (process.platform !== 'win32') {
  throw new Error('Windows SEA package must be built on Windows.');
}
if (Number.parseInt(process.versions.node.split('.')[0] ?? '0', 10) < 24) {
  throw new Error('Windows SEA packaging requires Node.js 24 or newer.');
}

await rm(intermediateDirectory, { recursive: true, force: true });
await mkdir(appDirectory, { recursive: true });
await mkdir(releaseDirectory, { recursive: true });
await rm(portableArchive, { force: true });

await build({
  entryPoints: [join(projectRoot, 'src', 'index.ts')],
  outfile: bundledEntry,
  bundle: true,
  format: 'cjs',
  platform: 'node',
  target: 'node24',
  sourcemap: false,
  minify: true,
  alias: {
    'node-gyp-build': join(projectRoot, 'scripts', 'selection-hook-native-loader.cjs'),
  },
  define: {
    'import.meta.url': '__filename',
    'process.env.SELECT_BRIDGE_PACKAGED': '"1"',
  },
  banner: { js: '// SelectBridge — bundled for Node.js SEA' },
});

await writeFile(
  seaConfig,
  `${JSON.stringify(
    {
      main: bundledEntry,
      output: seaBlob,
      disableExperimentalSEAWarning: true,
      useSnapshot: false,
      useCodeCache: false,
    },
    null,
    2,
  )}\n`,
  'utf8',
);

const { spawnSync } = await import('node:child_process');
const seaResult = spawnSync(process.execPath, ['--experimental-sea-config', seaConfig], {
  cwd: projectRoot,
  encoding: 'utf8',
  stdio: 'inherit',
});
if (seaResult.error) {
  throw seaResult.error;
}
if (seaResult.status !== 0) {
  throw new Error(`Node SEA blob generation failed with exit code ${seaResult.status}.`);
}

await copyFile(process.execPath, executablePath);
await rcedit(executablePath, {
  icon: iconPath,
  'application-manifest': manifestPath,
  'file-version': version,
  'product-version': version,
  'version-string': {
    FileDescription: 'SelectBridge',
    InternalName: 'SelectBridge',
    OriginalFilename: 'SelectBridge.exe',
    ProductName: 'SelectBridge',
  },
});
await inject(executablePath, 'NODE_SEA_BLOB', await readFile(seaBlob), {
  sentinelFuse: 'NODE_SEA_FUSE_fce680ab2cc467b6e072b8b5df1996b2',
});
await setWindowsSubsystem(executablePath, 2);

await copyFile(nativeAddonPath, join(appDirectory, 'select_bridge_win32_ui.node'));
await copyFile(selectionHookNativePath, join(appDirectory, 'selection-hook.node'));

await mkdir(portableDirectory, { recursive: true });
for (const fileName of [
  'SelectBridge.exe',
  'selection-hook.node',
  'select_bridge_win32_ui.node',
]) {
  await copyFile(join(appDirectory, fileName), join(portableDirectory, fileName));
}
await writeFile(join(portableDirectory, 'portable.flag'), '', 'utf8');

const portableResult = createPortableArchive(portableArchive, portableRoot);
if (portableResult.status !== 0) {
  throw new Error(`Portable ZIP creation failed with exit code ${portableResult.status}.`);
}

console.log(`Windows app staging created: ${appDirectory}`);
console.log(`Portable archive created: ${portableArchive}`);

function createPortableArchive(archivePath, sourceRoot) {
  const sevenZip = spawnSync(
    '7z.exe',
    ['a', '-tzip', '-mx=9', '-mmt=on', archivePath, 'SelectBridge'],
    { cwd: sourceRoot, encoding: 'utf8', stdio: 'inherit' },
  );
  if (!sevenZip.error) {
    return sevenZip;
  }
  if (sevenZip.error.code !== 'ENOENT') {
    throw sevenZip.error;
  }

  const tar = spawnSync(
    'tar.exe',
    ['-a', '-c', '-f', archivePath, '-C', sourceRoot, 'SelectBridge'],
    { cwd: projectRoot, encoding: 'utf8', stdio: 'inherit' },
  );
  if (tar.error) {
    throw tar.error;
  }
  return tar;
}

async function setWindowsSubsystem(executable, subsystem) {
  const image = await readFile(executable);
  if (image.length < 0x40 || image.readUInt16LE(0) !== 0x5a4d) {
    throw new Error(`${executable} is not a valid PE image.`);
  }

  const peOffset = image.readUInt32LE(0x3c);
  if (peOffset + 24 + 70 > image.length || image.readUInt32LE(peOffset) !== 0x00004550) {
    throw new Error(`${executable} has an invalid PE header.`);
  }

  const optionalHeader = peOffset + 24;
  const magic = image.readUInt16LE(optionalHeader);
  if (magic !== 0x10b && magic !== 0x20b) {
    throw new Error(`${executable} has an unsupported PE optional header.`);
  }

  image.writeUInt16LE(subsystem, optionalHeader + 68);
  await writeFile(executable, image);
}
