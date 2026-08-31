import { existsSync } from 'node:fs';
import { dirname, join } from 'node:path';

export type DistributionMode = 'development' | 'setup' | 'portable';

export function getDistributionMode(): DistributionMode {
  if (process.env.SELECT_BRIDGE_PACKAGED !== '1') {
    return 'development';
  }

  return existsSync(join(dirname(process.execPath), 'portable.flag')) ? 'portable' : 'setup';
}
