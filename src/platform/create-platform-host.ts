import type { RuntimeMode } from '../config.js';

import { HeadlessHost } from './headless-host.js';
import type { PlatformHost } from './types.js';

export async function createPlatformHost(mode: RuntimeMode): Promise<PlatformHost> {
  if (mode !== 'tray') {
    return new HeadlessHost();
  }

  if (process.platform !== 'win32') {
    console.warn('[platform] 托盘模式目前优先支持 Windows，已回退到 headless。');
    return new HeadlessHost();
  }

  try {
    const { WindowsNativeHost } = await import('./windows/windows-native-host.js');
    return await WindowsNativeHost.load();
  } catch (error: unknown) {
    console.warn(
      '[platform] 无法加载 Windows 原生模块，已回退到 headless。请运行 pnpm build:native。',
      error,
    );
    return new HeadlessHost();
  }
}
