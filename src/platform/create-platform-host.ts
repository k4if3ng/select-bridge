import type { HostMode } from '../config.js';
import type { PlatformHost } from './types.js';
import { HeadlessHost } from './headless-host.js';

export function getDefaultHostMode(): HostMode {
  return process.platform === 'win32' ? 'native' : 'headless';
}

export async function createPlatformHost(
  hostMode: HostMode = getDefaultHostMode(),
): Promise<PlatformHost> {
  if (hostMode === 'headless') {
    return new HeadlessHost();
  }

  if (process.platform !== 'win32') {
    throw new Error(
      `当前平台 ${process.platform} 尚未提供 native 宿主，请使用 --host=headless。`,
    );
  }

  try {
    const { WindowsNativeHost } = await import('./windows/windows-native-host.js');
    return await WindowsNativeHost.load();
  } catch (error: unknown) {
    const detail = error instanceof Error ? error.message : String(error);
    throw new Error(
      `无法加载 Windows native 宿主，请运行 pnpm build:native。若需要无界面运行，请显式使用 --host=headless。${detail ? ` ${detail}` : ''}`,
    );
  }
}
