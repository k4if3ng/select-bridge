import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { homedir } from 'node:os';
import { dirname, join } from 'node:path';

export const TRIGGER_MODES = [
  'immediate',
  'icon',
  'dot',
  'ctrl',
  'alt',
  'shift',
  'custom',
] as const;
export type TriggerMode = (typeof TRIGGER_MODES)[number];
export type IndicatorAction = 'click' | 'hover';
export type RuntimeMode = 'headless' | 'tray';

export interface AppConfig {
  enabled: boolean;
  triggerMode: TriggerMode;
  indicatorAction: IndicatorAction;
  maxTextLength: number;
  dedupeWindowMs: number;
  selectionStableMs: number;
  indicatorTtlMs: number;
  hoverDelayMs: number;
  iconSize: number;
  dotSize: number;
  iconPath: string;
  customShortcut: string;
  autoStart: boolean;
}

export interface RuntimeOptions {
  mode: RuntimeMode;
  silent: boolean;
  triggerMode?: TriggerMode;
  customShortcut?: string;
}

export const DEFAULT_CONFIG: Readonly<AppConfig> = {
  enabled: true,
  triggerMode: 'immediate',
  indicatorAction: 'click',
  maxTextLength: 200,
  dedupeWindowMs: 800,
  selectionStableMs: 120,
  indicatorTtlMs: 3000,
  hoverDelayMs: 450,
  iconSize: 24,
  dotSize: 12,
  iconPath: '',
  customShortcut: 'Ctrl+Alt+G',
  autoStart: false,
};

export class ConfigStore {
  readonly path: string;

  constructor(path = resolveConfigPath()) {
    this.path = path;
  }

  async load(): Promise<AppConfig> {
    let stored: unknown;

    try {
      stored = JSON.parse(await readFile(this.path, 'utf8')) as unknown;
    } catch (error: unknown) {
      if (isNodeError(error) && error.code === 'ENOENT') {
        return applyEnvironment(DEFAULT_CONFIG);
      }

      console.warn(`[config] 无法读取 ${this.path}，使用默认配置。`, error);
      return applyEnvironment(DEFAULT_CONFIG);
    }

    return applyEnvironment(sanitizeConfig(stored));
  }

  async save(config: AppConfig): Promise<void> {
    await mkdir(dirname(this.path), { recursive: true });
    await writeFile(this.path, `${JSON.stringify(config, null, 2)}\n`, 'utf8');
  }
}

export function loadRuntimeOptions(argv: string[]): RuntimeOptions {
  const forceHeadless = argv.includes('--headless');
  const silent = argv.includes('--silent');
  const wantsTray = argv.includes('--tray') || silent;
  const triggerValue = argv.find((argument) => argument.startsWith('--trigger='))?.slice('--trigger='.length);
  const customShortcut = argv
    .find((argument) => argument.startsWith('--shortcut='))
    ?.slice('--shortcut='.length)
    .trim();

  return {
    mode: forceHeadless ? 'headless' : wantsTray ? 'tray' : 'headless',
    silent,
    triggerMode: isTriggerMode(triggerValue) ? triggerValue : undefined,
    customShortcut: customShortcut || undefined,
  };
}

export function isTriggerMode(value: unknown): value is TriggerMode {
  return typeof value === 'string' && (TRIGGER_MODES as readonly string[]).includes(value);
}

function sanitizeConfig(value: unknown): AppConfig {
  if (!isRecord(value)) {
    return { ...DEFAULT_CONFIG };
  }

  return {
    enabled: getBoolean(value.enabled, DEFAULT_CONFIG.enabled),
    triggerMode: isTriggerMode(value.triggerMode) ? value.triggerMode : DEFAULT_CONFIG.triggerMode,
    indicatorAction:
      value.indicatorAction === 'hover' || value.indicatorAction === 'click'
        ? value.indicatorAction
        : DEFAULT_CONFIG.indicatorAction,
    maxTextLength: getPositiveInteger(value.maxTextLength, DEFAULT_CONFIG.maxTextLength),
    dedupeWindowMs: getPositiveInteger(value.dedupeWindowMs, DEFAULT_CONFIG.dedupeWindowMs),
    selectionStableMs: getPositiveInteger(value.selectionStableMs, DEFAULT_CONFIG.selectionStableMs),
    indicatorTtlMs: getPositiveInteger(value.indicatorTtlMs, DEFAULT_CONFIG.indicatorTtlMs),
    hoverDelayMs: getPositiveInteger(value.hoverDelayMs, DEFAULT_CONFIG.hoverDelayMs),
    iconSize: getBoundedInteger(value.iconSize, DEFAULT_CONFIG.iconSize, 12, 64),
    dotSize: getBoundedInteger(value.dotSize, DEFAULT_CONFIG.dotSize, 6, 32),
    iconPath: typeof value.iconPath === 'string' ? value.iconPath : DEFAULT_CONFIG.iconPath,
    customShortcut:
      typeof value.customShortcut === 'string' && value.customShortcut.trim()
        ? value.customShortcut.trim()
        : DEFAULT_CONFIG.customShortcut,
    autoStart: getBoolean(value.autoStart, DEFAULT_CONFIG.autoStart),
  };
}

function applyEnvironment(config: Readonly<AppConfig>): AppConfig {
  return {
    ...config,
    maxTextLength: getEnvironmentInteger(
      'SELECTION_FORWARD_MAX_LENGTH',
      config.maxTextLength,
    ),
    dedupeWindowMs: getEnvironmentInteger(
      'SELECTION_FORWARD_DEDUPE_MS',
      config.dedupeWindowMs,
    ),
    selectionStableMs: getEnvironmentInteger(
      'SELECTION_FORWARD_STABLE_MS',
      config.selectionStableMs,
    ),
    indicatorTtlMs: getEnvironmentInteger(
      'SELECTION_FORWARD_INDICATOR_TTL_MS',
      config.indicatorTtlMs,
    ),
    hoverDelayMs: getEnvironmentInteger(
      'SELECTION_FORWARD_HOVER_MS',
      config.hoverDelayMs,
    ),
    iconSize: getBoundedEnvironmentInteger(
      'SELECTION_FORWARD_ICON_SIZE',
      config.iconSize,
      12,
      64,
    ),
    dotSize: getBoundedEnvironmentInteger(
      'SELECTION_FORWARD_DOT_SIZE',
      config.dotSize,
      6,
      32,
    ),
    iconPath: process.env.SELECTION_FORWARD_ICON_PATH ?? config.iconPath,
    customShortcut:
      process.env.SELECTION_FORWARD_SHORTCUT?.trim() || config.customShortcut,
    triggerMode: isTriggerMode(process.env.SELECTION_FORWARD_TRIGGER_MODE)
      ? process.env.SELECTION_FORWARD_TRIGGER_MODE
      : config.triggerMode,
  };
}

function resolveConfigPath(): string {
  if (process.env.SELECTION_FORWARD_CONFIG) {
    return process.env.SELECTION_FORWARD_CONFIG;
  }

  const baseDirectory =
    process.platform === 'win32'
      ? process.env.APPDATA ?? join(homedir(), 'AppData', 'Roaming')
      : process.env.XDG_CONFIG_HOME ?? join(homedir(), '.config');

  return join(baseDirectory, 'selection-forward', 'config.json');
}

function getEnvironmentInteger(name: string, fallback: number): number {
  return getPositiveInteger(Number.parseInt(process.env[name] ?? '', 10), fallback);
}

function getBoundedEnvironmentInteger(
  name: string,
  fallback: number,
  minimum: number,
  maximum: number,
): number {
  return getBoundedInteger(
    Number.parseInt(process.env[name] ?? '', 10),
    fallback,
    minimum,
    maximum,
  );
}

function getPositiveInteger(value: unknown, fallback: number): number {
  return typeof value === 'number' && Number.isSafeInteger(value) && value > 0 ? value : fallback;
}

function getBoundedInteger(
  value: unknown,
  fallback: number,
  minimum: number,
  maximum: number,
): number {
  return typeof value === 'number' && Number.isSafeInteger(value)
    ? Math.min(maximum, Math.max(minimum, value))
    : fallback;
}

function getBoolean(value: unknown, fallback: boolean): boolean {
  return typeof value === 'boolean' ? value : fallback;
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}

function isNodeError(error: unknown): error is NodeJS.ErrnoException {
  return error instanceof Error;
}
