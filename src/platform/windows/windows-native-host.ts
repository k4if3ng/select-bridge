import { createRequire } from 'node:module';
import { existsSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

import type {
  IndicatorOptions,
  PlatformCapabilities,
  PlatformEvent,
  PlatformHost,
  PlatformState,
  ShortcutRegistrationResult,
} from '../types.js';

const WINDOWS_CAPABILITIES: PlatformCapabilities = {
  hostMode: 'native',
  indicator: true,
  nativeShortcut: true,
  portableShortcut: false,
  autoStart: true,
};

interface NativeAddon {
  start(callback: (type: string, value?: string) => void): boolean;
  stop(): void;
  updateTray(
    enabled: boolean,
    triggerMode: string,
    autoStart: boolean,
    indicatorAction: string,
    iconSize: number,
    dotSize: number,
    customShortcut: string,
    targetMode: string,
    customTargetUrlTemplate: string,
    targetUrlOverrideSource: string,
  ): void;
  showIndicator(
    x: number | null,
    y: number | null,
    style: string,
    size: number,
    hoverEnabled: boolean,
    hoverDelayMs: number,
  ): void;
  hideIndicator(): void;
  openExternalUrl(url: string): boolean;
  openPath(path: string): boolean;
  completeTargetUrlSave(ok: boolean, message: string): void;
  showError(title: string, message: string): void;
  registerShortcut(shortcut: string): ShortcutRegistrationResult;
  setAutoStart(enabled: boolean, executablePath: string, argumentsText: string): boolean;
}

export class WindowsNativeHost implements PlatformHost {
  readonly capabilities = WINDOWS_CAPABILITIES;

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
    this.addon.updateTray(
      state.enabled,
      state.triggerMode,
      state.autoStart,
      state.indicatorAction,
      state.iconSize,
      state.dotSize,
      state.customShortcut,
      state.targetMode,
      state.customTargetUrlTemplate,
      state.targetUrlOverrideSource ?? '',
    );
  }

  showIndicator(options: IndicatorOptions): void {
    this.addon.showIndicator(
      options.x ?? null,
      options.y ?? null,
      options.style,
      options.size,
      options.hoverEnabled,
      options.hoverDelayMs,
    );
  }

  hideIndicator(): void {
    this.addon.hideIndicator();
  }

  openExternalUrl(url: string): boolean {
    return this.addon.openExternalUrl(url);
  }

  openPath(path: string): boolean {
    return this.addon.openPath(path);
  }

  completeTargetUrlSave(ok: boolean, message = ''): void {
    this.addon.completeTargetUrlSave(ok, message);
  }

  showError(title: string, message: string): void {
    this.addon.showError(title, message);
  }

  registerShortcut(shortcut: string): ShortcutRegistrationResult {
    return this.addon.registerShortcut(shortcut);
  }

  async setAutoStart(enabled: boolean): Promise<boolean> {
    const scriptArgument = process.argv[1] ?? '';
    const scriptPath = /\.(?:c|m)?js$/i.test(scriptArgument) ? resolve(scriptArgument) : '';
    const argumentsText = scriptPath ? `"${scriptPath}" --silent` : '--silent';
    return this.addon.setAutoStart(enabled, process.execPath, argumentsText);
  }
}

function resolveNativeModulePath(): string {
  if (process.env.SELECT_BRIDGE_NATIVE_PATH) {
    return process.env.SELECT_BRIDGE_NATIVE_PATH;
  }

  const packagedPath = resolve(dirname(process.execPath), 'select_bridge_win32_ui.node');
  if (existsSync(packagedPath)) {
    return packagedPath;
  }
  const currentDirectory = dirname(fileURLToPath(import.meta.url));
  return resolve(
    currentDirectory,
    '../../../native/win32/build/Release/select_bridge_win32_ui.node',
  );
}

function toPlatformEvent(type: string, value?: string): PlatformEvent | undefined {
  switch (type) {
    case 'indicator-click':
    case 'indicator-hover':
    case 'toggle-enabled':
    case 'toggle-auto-start':
    case 'shortcut':
    case 'open-config-file':
    case 'open-config-directory':
    case 'reload-config':
    case 'remove-custom-shortcut':
    case 'exit':
      return { type };
    case 'set-trigger-mode':
    case 'set-indicator-action':
    case 'set-icon-size':
    case 'set-dot-size':
    case 'set-custom-shortcut':
    case 'set-custom-shortcut-and-activate':
    case 'shortcut-conflict':
    case 'native-error':
    case 'set-target-mode':
    case 'save-target-url':
      return { type, value: value ?? '' };
    default:
      console.warn(`[platform] 未知原生事件：${type}`);
      return undefined;
  }
}
