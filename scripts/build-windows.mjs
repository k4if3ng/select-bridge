import { cp, copyFile, mkdir, readFile, realpath, rm, writeFile } from 'node:fs/promises';
import { createRequire } from 'node:module';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

import { build } from 'esbuild';
import { rcedit } from 'rcedit';

const require = createRequire(import.meta.url);
const { inject } = require('postject');

const projectRoot = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const intermediateDirectory = join(projectRoot, 'build', 'windows');
const releaseDirectory = join(projectRoot, 'release', 'windows-x64');
const bundledEntry = join(intermediateDirectory, 'main.cjs');
const seaBlob = join(intermediateDirectory, 'sea-prep.blob');
const seaConfig = join(intermediateDirectory, 'sea-config.json');
const executablePath = join(releaseDirectory, 'SelectionForward.exe');
const iconPath = join(projectRoot, 'resources', 'icon.ico');
const nativeAddonPath = join(
  projectRoot,
  'native',
  'win32',
  'build',
  'Release',
  'selection_forward_win32_ui.node',
);
const trayLauncherPath = join(
  projectRoot,
  'native',
  'win32',
  'build',
  'Release',
  'selection_forward_tray_launcher.exe',
);

if (process.platform !== 'win32') {
  throw new Error('Windows SEA package must be built on Windows.');
}
if (Number.parseInt(process.versions.node.split('.')[0] ?? '0', 10) < 24) {
  throw new Error('Windows SEA packaging requires Node.js 24 or newer.');
}

await rm(intermediateDirectory, { recursive: true, force: true });
await rm(releaseDirectory, { recursive: true, force: true });
await mkdir(intermediateDirectory, { recursive: true });
await mkdir(releaseDirectory, { recursive: true });

await build({
  entryPoints: [join(projectRoot, 'src', 'index.ts')],
  outfile: bundledEntry,
  bundle: true,
  format: 'cjs',
  platform: 'node',
  target: 'node24',
  sourcemap: false,
  minify: true,
  packages: 'external',
  define: {
    'import.meta.url': '__filename',
  },
  banner: { js: '// Selection Forward — bundled for Node.js SEA' },
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
  'file-version': '1.0.0',
  'product-version': '1.0.0',
  'version-string': {
    FileDescription: 'Selection Forward',
    InternalName: 'SelectionForward',
    OriginalFilename: 'SelectionForward.exe',
    ProductName: 'Selection Forward',
  },
});
await inject(executablePath, 'NODE_SEA_BLOB', await readFile(seaBlob), {
  sentinelFuse: 'NODE_SEA_FUSE_fce680ab2cc467b6e072b8b5df1996b2',
});

await copyFile(nativeAddonPath, join(releaseDirectory, 'selection_forward_win32_ui.node'));
const packagedTrayLauncher = join(releaseDirectory, 'SelectionForwardTray.exe');
await copyFile(trayLauncherPath, packagedTrayLauncher);
await rcedit(packagedTrayLauncher, {
  icon: iconPath,
  'file-version': '1.0.0',
  'product-version': '1.0.0',
  'version-string': {
    FileDescription: 'Selection Forward Tray Launcher',
    InternalName: 'SelectionForwardTray',
    OriginalFilename: 'SelectionForwardTray.exe',
    ProductName: 'Selection Forward',
  },
});
await setWindowsSubsystem(packagedTrayLauncher, 2);
await copyFile(iconPath, join(releaseDirectory, 'icon.ico'));

const selectionHookEntry = require.resolve('selection-hook');
await copyRuntimePackage('selection-hook', selectionHookEntry, [
  'index.js',
  'package.json',
  'LICENSE',
  'README.md',
  join('prebuilds', 'win32-x64'),
]);
const selectionHookRequire = createRequire(selectionHookEntry);
await copyRuntimePackage('node-gyp-build', selectionHookRequire.resolve('node-gyp-build'), [
  'index.js',
  'node-gyp-build.js',
  'package.json',
  'LICENSE',
  'README.md',
]);

console.log(`Windows package created: ${releaseDirectory}`);

async function copyRuntimePackage(packageName, packageEntry, entries) {
  const sourceDirectory = await realpath(dirname(packageEntry));
  const destinationDirectory = join(releaseDirectory, 'node_modules', packageName);
  await mkdir(destinationDirectory, { recursive: true });

  for (const entry of entries) {
    const destination = join(destinationDirectory, entry);
    await mkdir(dirname(destination), { recursive: true });
    await cp(join(sourceDirectory, entry), destination, {
      recursive: true,
      force: true,
    });
  }
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
