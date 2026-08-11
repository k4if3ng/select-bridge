import { createRequire } from 'node:module';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

import type {
  IndicatorOptions,
  PlatformEvent,
  PlatformHost,
  PlatformState,
} from '../types.js';

interface NativeAddon {
  start(callback: (type: string, value?: string) => void): boolean;
  stop(): void;
  updateTray(enabled: boolean, triggerMode: string, autoStart: boolean): void;
  showIndicator(
    x: number | null,
    y: number | null,
    style: string,
    hoverEnabled: boolean,
    hoverDelayMs: number,
  ): void;
  hideIndicator(): void;
  setAutoStart(enabled: boolean, executablePath: string, argumentsText: string): boolean;
}

export class WindowsNativeHost implements PlatformHost {
  readonly kind = 'tray' as const;

  private constructor(private readonly addon: NativeAddon) {}

  static async load(): Promise<WindowsNativeHost> {
    const require = createRequire(import.meta.url);
    const modulePath = resolveNativeModulePath();
    const addon = require(modulePath) as NativeAddon;
    return new WindowsNativeHost(addon);
  }

  async start(onEvent: (event: PlatformEvent) => void): Promise<void> {
    const started = this.addon.start((type, value) => {
      const event = toPlatformEvent(type, value);
      if (event) {
        onEvent(event);
      }
    });

    if (!started) {
      throw new Error('Windows 原生平台层启动失败');
    }
  }

  async stop(): Promise<void> {
    this.addon.stop();
  }

  updateState(state: PlatformState): void {
    this.addon.updateTray(state.enabled, state.triggerMode, state.autoStart);
  }

  showIndicator(options: IndicatorOptions): void {
    this.addon.showIndicator(
      options.x ?? null,
      options.y ?? null,
      options.style,
      options.hoverEnabled,
      options.hoverDelayMs,
    );
  }

  hideIndicator(): void {
    this.addon.hideIndicator();
  }

  async setAutoStart(enabled: boolean): Promise<boolean> {
    const scriptPath = process.argv[1] ? resolve(process.argv[1]) : '';
    const executablePath = process.execPath;
    const argumentsText = scriptPath ? `"${scriptPath}" --silent` : '--silent';
    return this.addon.setAutoStart(enabled, executablePath, argumentsText);
  }
}

function resolveNativeModulePath(): string {
  if (process.env.SELECTION_FORWARD_NATIVE_PATH) {
    return process.env.SELECTION_FORWARD_NATIVE_PATH;
  }

  const currentDirectory = dirname(fileURLToPath(import.meta.url));
  return resolve(
    currentDirectory,
    '../../../native/win32/build/Release/selection_forward_win32.node',
  );
}

function toPlatformEvent(type: string, value?: string): PlatformEvent | undefined {
  switch (type) {
    case 'indicator-click':
    case 'indicator-hover':
    case 'toggle-enabled':
    case 'toggle-auto-start':
    case 'open-settings':
    case 'exit':
      return { type };
    case 'set-trigger-mode':
      return { type, value: value ?? '' };
    default:
      console.warn(`[platform] 未知原生事件：${type}`);
      return undefined;
  }
}
