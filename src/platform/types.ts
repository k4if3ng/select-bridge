import type { TriggerMode } from '../config.js';

export interface IndicatorOptions {
  x?: number;
  y?: number;
  style: 'icon' | 'dot';
  hoverEnabled: boolean;
  hoverDelayMs: number;
}

export interface PlatformState {
  enabled: boolean;
  triggerMode: TriggerMode;
  autoStart: boolean;
}

export type PlatformEvent =
  | { type: 'indicator-click' }
  | { type: 'indicator-hover' }
  | { type: 'toggle-enabled' }
  | { type: 'set-trigger-mode'; value: string }
  | { type: 'toggle-auto-start' }
  | { type: 'open-settings' }
  | { type: 'exit' };

export interface PlatformHost {
  readonly kind: 'headless' | 'tray';
  start(onEvent: (event: PlatformEvent) => void): Promise<void>;
  stop(): Promise<void>;
  updateState(state: PlatformState): void;
  showIndicator(options: IndicatorOptions): void;
  hideIndicator(): void;
  setAutoStart(enabled: boolean): Promise<boolean>;
}
