import type {
  IndicatorOptions,
  PlatformEvent,
  PlatformHost,
  PlatformState,
  ShortcutRegistrationResult,
} from './types.js';

export class HeadlessHost implements PlatformHost {
  readonly kind = 'headless' as const;

  async start(_onEvent: (event: PlatformEvent) => void): Promise<void> {}
  async stop(): Promise<void> {}
  updateState(_state: PlatformState): void {}
  showIndicator(_options: IndicatorOptions): void {}
  hideIndicator(): void {}
  openExternalUrl(_url: string): boolean {
    return false;
  }

  registerShortcut(_shortcut: string): ShortcutRegistrationResult {
    return { ok: false, errorCode: 0, normalized: '' };
  }

  async setAutoStart(_enabled: boolean): Promise<boolean> {
    return false;
  }
}
