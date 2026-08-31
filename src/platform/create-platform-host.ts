import type { PlatformHost } from './types.js';

export async function createPlatformHost(): Promise<PlatformHost> {
  if (process.platform !== 'win32') {
    throw new Error('Selection Forward 托盘版目前仅支持 Windows。');
  }

  try {
    const { WindowsNativeHost } = await import('./windows/windows-native-host.js');
    return await WindowsNativeHost.load();
  } catch (error: unknown) {
    const detail = error instanceof Error ? error.message : String(error);
    throw new Error(
      `无法加载 Windows 原生模块，请运行 pnpm build:native。${detail ? ` ${detail}` : ''}`,
    );
  }
}
