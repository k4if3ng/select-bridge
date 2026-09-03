import type {
  IndicatorOptions,
  PlatformCapabilities,
  PlatformEvent,
  PlatformHost,
  PlatformState,
  ShortcutRegistrationResult,
} from './types.js';

const HEADLESS_CAPABILITIES: PlatformCapabilities = {
  hostMode: 'headless',
  indicator: false,
  nativeShortcut: false,
  portableShortcut: true,
  autoStart: false,
};

export class HeadlessHost implements PlatformHost {
  readonly capabilities = HEADLESS_CAPABILITIES;

  async start(_onEvent: (event: PlatformEvent) => void): Promise<void> {}

  async stop(): Promise<void> {}

  updateState(_state: PlatformState): void {}

  showIndicator(_options: IndicatorOptions): void {}

  hideIndicator(): void {}

  openExternalUrl(_url: string): boolean {
    return false;
  }

  openPath(_path: string): boolean {
    return false;
  }

  completeTargetUrlSave(_ok: boolean, _message?: string): void {}

  showError(title: string, message: string): void {
    console.error(`[${title}] ${message}`);
  }

  registerShortcut(shortcut: string): ShortcutRegistrationResult {
    return {
      ok: shortcut.length === 0,
      errorCode: shortcut.length === 0 ? 0 : -1,
      normalized: shortcut,
    };
  }

  async setAutoStart(_enabled: boolean): Promise<boolean> {
    return false;
  }
}
