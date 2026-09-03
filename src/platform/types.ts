import type {
  HostMode,
  TargetMode,
  TargetUrlOverrideSource,
  TriggerMode,
} from '../config.js';
import type { UiLanguage } from '../i18n.js';

export interface IndicatorOptions {
  x?: number;
  y?: number;
  style: 'icon' | 'dot';
  size: number;
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
  customShortcut: string;
  targetMode: TargetMode;
  customTargetUrlTemplate: string;
  targetUrlOverrideSource?: TargetUrlOverrideSource;
  uiLanguage: UiLanguage;
}

export interface ShortcutRegistrationResult {
  ok: boolean;
  errorCode: number;
  normalized: string;
}

export interface PlatformCapabilities {
  hostMode: HostMode;
  indicator: boolean;
  nativeShortcut: boolean;
  portableShortcut: boolean;
  autoStart: boolean;
}

export type PlatformEvent =
  | { type: 'indicator-click' }
  | { type: 'indicator-hover' }
  | { type: 'toggle-enabled' }
  | { type: 'set-trigger-mode'; value: string }
  | { type: 'toggle-auto-start' }
  | { type: 'set-indicator-action'; value: string }
  | { type: 'set-icon-size'; value: string }
  | { type: 'set-dot-size'; value: string }
  | { type: 'set-custom-shortcut'; value: string }
  | { type: 'set-custom-shortcut-and-activate'; value: string }
  | { type: 'remove-custom-shortcut' }
  | { type: 'shortcut' }
  | { type: 'shortcut-conflict'; value: string }
  | { type: 'native-error'; value: string }
  | { type: 'set-target-mode'; value: string }
  | { type: 'save-target-url'; value: string }
  | { type: 'set-ui-language'; value: string }
  | { type: 'open-config-file' }
  | { type: 'open-config-directory' }
  | { type: 'reload-config' }
  | { type: 'exit' };

export interface PlatformHost {
  readonly capabilities: PlatformCapabilities;
  start(onEvent: (event: PlatformEvent) => void): Promise<void>;
  stop(): Promise<void>;
  getSystemUiLanguage(): string;
  updateState(state: PlatformState): void;
  showIndicator(options: IndicatorOptions): void;
  hideIndicator(): void;
  openExternalUrl(url: string): boolean;
  openPath(path: string): boolean;
  completeTargetUrlSave(ok: boolean, message?: string): void;
  showError(title: string, message: string): void;
  registerShortcut(shortcut: string): ShortcutRegistrationResult;
  setAutoStart(enabled: boolean): Promise<boolean>;
}
