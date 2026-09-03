import { readFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

interface PackageMetadata {
  version?: unknown;
}

export const APP_VERSION = process.env.SELECT_BRIDGE_APP_VERSION ?? readDevelopmentVersion();

function readDevelopmentVersion(): string {
  const packagePath = resolve(dirname(fileURLToPath(import.meta.url)), '..', 'package.json');
  const metadata = JSON.parse(readFileSync(packagePath, 'utf8')) as PackageMetadata;
  if (typeof metadata.version !== 'string' || !/^\d+\.\d+\.\d+$/.test(metadata.version)) {
    throw new Error(`Invalid SelectBridge version in ${packagePath}`);
  }
  return metadata.version;
}
