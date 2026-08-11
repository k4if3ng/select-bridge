import type { TriggerMode } from '../config.js';

export interface IndicatorOptions {
  x?: number;
  y?: number;
  style: 'icon' | 'dot';
  size: number;
  iconPath: string;
  hoverEnabled: boolean;
  hoverDelayMs: number;
}

export interface PlatformState {
  enabled: boolean;
  triggerMode: TriggerMode;
  autoStart: boolean;
  indicatorAction: 'click' | 'hover';
  iconSize: number;
  dotSize: number;
  iconPath: string;
  customShortcut: string;
}

export interface ShortcutRegistrationResult {
  ok: boolean;
  errorCode: number;
  normalized: string;
}

export type PlatformEvent =
  | { type: 'indicator-click' }
  | { type: 'indicator-hover' }
  | { type: 'toggle-enabled' }
  | { type: 'set-trigger-mode'; value: string }
  | { type: 'toggle-auto-start' }
  | { type: 'toggle-hover' }
  | { type: 'set-icon-size'; value: string }
  | { type: 'set-dot-size'; value: string }
  | { type: 'set-icon-path'; value: string }
  | { type: 'set-custom-shortcut'; value: string }
  | { type: 'shortcut' }
  | { type: 'shortcut-conflict'; value: string }
  | { type: 'open-settings' }
  | { type: 'exit' };

export interface PlatformHost {
  readonly kind: 'headless' | 'tray';
  start(onEvent: (event: PlatformEvent) => void): Promise<void>;
  stop(): Promise<void>;
  updateState(state: PlatformState): void;
  showIndicator(options: IndicatorOptions): void;
  hideIndicator(): void;
  registerShortcut(shortcut: string): ShortcutRegistrationResult;
  setAutoStart(enabled: boolean): Promise<boolean>;
}
