import { mkdir, readFile, rename, rm, writeFile } from 'node:fs/promises';
import { homedir } from 'node:os';
import { dirname, join } from 'node:path';

import { getDistributionMode } from './distribution.js';
import { isUiLanguage, type UiLanguage } from './i18n.js';

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
export const TARGET_MODES = ['goldendict', 'custom'] as const;
export type TargetMode = (typeof TARGET_MODES)[number];
export type TargetUrlOverrideSource = 'cli' | 'environment';

export const HOST_MODES = ['native', 'headless'] as const;
export type HostMode = (typeof HOST_MODES)[number];

export const DEFAULT_TARGET_URL_TEMPLATE = 'goldendict://{text}?target=popup';

export interface AppConfig {
  schemaVersion: number;
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
  customShortcut: string;
  autoStart: boolean;
  targetMode: TargetMode;
  customTargetUrlTemplate: string;
  uiLanguage: UiLanguage;
}

export interface RuntimeOptions {
  triggerMode?: TriggerMode;
  customShortcut?: string;
  hostMode?: HostMode;
  targetUrlOverride?: string;
  targetUrlOverrideSource?: TargetUrlOverrideSource;
}

export const DEFAULT_CONFIG: Readonly<AppConfig> = {
  schemaVersion: 10,
  enabled: true,
  triggerMode: 'immediate',
  indicatorAction: 'click',
  maxTextLength: 200,
  dedupeWindowMs: 800,
  selectionStableMs: 60,
  indicatorTtlMs: 3000,
  hoverDelayMs: 350,
  iconSize: 32,
  dotSize: 16,
  customShortcut: 'Ctrl+Alt+G',
  autoStart: false,
  targetMode: 'goldendict',
  customTargetUrlTemplate: '',
  uiLanguage: 'en-US',
};

export class ConfigStore {
  readonly path: string;
  private saveQueue: Promise<void> = Promise.resolve();

  constructor(
    path = resolveConfigPath(),
    private readonly initialUiLanguage: UiLanguage = DEFAULT_CONFIG.uiLanguage,
  ) {
    this.path = path;
  }

  async load(): Promise<AppConfig> {
    try {
      let stored: unknown;
      try {
        stored = JSON.parse(await readFile(this.path, 'utf8')) as unknown;
      } catch (error: unknown) {
        if (!isNodeError(error) || error.code !== 'ENOENT') {
          throw error;
        }
        const created = { ...DEFAULT_CONFIG, uiLanguage: this.initialUiLanguage };
        await this.save(created);
        return applyEnvironment(created);
      }

      if (isRecord(stored) && !Object.hasOwn(stored, 'uiLanguage')) {
        const migrated = sanitizeConfig(stored, this.initialUiLanguage);
        try {
          return applyEnvironment(
            await this.mergePersistent({ uiLanguage: this.initialUiLanguage }),
          );
        } catch (error: unknown) {
          console.warn(`[config] 无法持久化界面语言迁移到 ${this.path}。`, error);
          return applyEnvironment(migrated);
        }
      }
      return applyEnvironment(sanitizeConfig(stored, this.initialUiLanguage));
    } catch (error: unknown) {
      console.warn(`[config] 无法读取 ${this.path}，使用默认配置。`, error);
      return applyEnvironment({ ...DEFAULT_CONFIG, uiLanguage: this.initialUiLanguage });
    }
  }

  async readPersistent(): Promise<AppConfig> {
    try {
      return sanitizeConfig(
        JSON.parse(await readFile(this.path, 'utf8')) as unknown,
        this.initialUiLanguage,
      );
    } catch (error: unknown) {
      if (isNodeError(error) && error.code === 'ENOENT') {
        return { ...DEFAULT_CONFIG, uiLanguage: this.initialUiLanguage };
      }
      throw error;
    }
  }

  async ensureExists(): Promise<void> {
    try {
      await readFile(this.path, 'utf8');
    } catch (error: unknown) {
      if (!isNodeError(error) || error.code !== 'ENOENT') {
        throw error;
      }
      await this.save({ ...DEFAULT_CONFIG, uiLanguage: this.initialUiLanguage });
    }
  }

  async mergePersistent(patch: Partial<AppConfig>): Promise<AppConfig> {
    return this.enqueueSave(async () => {
      let stored: unknown = { ...DEFAULT_CONFIG, uiLanguage: this.initialUiLanguage };
      try {
        stored = JSON.parse(await readFile(this.path, 'utf8')) as unknown;
      } catch (error: unknown) {
        if (!isNodeError(error) || error.code !== 'ENOENT') {
          throw error;
        }
      }

      const storedRecord = isRecord(stored) ? stored : {};
      const updated = sanitizeConfig(
        { ...storedRecord, ...patch, schemaVersion: DEFAULT_CONFIG.schemaVersion },
        this.initialUiLanguage,
      );
      const persistedRecord: Record<string, unknown> = { ...storedRecord, ...updated };
      delete persistedRecord.targetUrlTemplate;
      await this.writeAtomically(persistedRecord as unknown as AppConfig);
      return updated;
    });
  }

  async save(config: AppConfig): Promise<void> {
    return this.enqueueSave(() => this.writeAtomically(config));
  }

  async flush(): Promise<void> {
    await this.saveQueue;
  }

  private async writeAtomically(config: AppConfig): Promise<void> {
    await mkdir(dirname(this.path), { recursive: true });
    const temporaryPath = `${this.path}.${process.pid}.${Date.now()}.tmp`;
    const contents = `${JSON.stringify(config, null, 2)}\n`;

    try {
      await writeFile(temporaryPath, contents, 'utf8');
      try {
        await rename(temporaryPath, this.path);
      } catch (error: unknown) {
        if (!isNodeError(error) || !['EEXIST', 'EPERM'].includes(error.code ?? '')) {
          throw error;
        }

        await rm(this.path, { force: true });
        await rename(temporaryPath, this.path);
      }
    } finally {
      await rm(temporaryPath, { force: true });
    }
  }

  private enqueueSave<T>(operation: () => Promise<T>): Promise<T> {
    const queued = this.saveQueue.then(operation);
    this.saveQueue = queued.then(() => undefined, () => undefined);
    return queued;
  }
}

export function loadRuntimeOptions(argv: string[]): RuntimeOptions {
  const triggerValue = argv.find((argument) => argument.startsWith('--trigger='))?.slice('--trigger='.length);
  const customShortcut = argv
    .find((argument) => argument.startsWith('--shortcut='))
    ?.slice('--shortcut='.length)
    .trim();
  const hostValue = argv
    .find((argument) => argument.startsWith('--host='))
    ?.slice('--host='.length);
  const cliTargetUrl = argv
    .find((argument) => argument.startsWith('--target-url='))
    ?.slice('--target-url='.length)
    .trim();
  const environmentTargetUrl = process.env.SELECT_BRIDGE_TARGET_URL?.trim();
  const targetUrlOverride = cliTargetUrl || environmentTargetUrl || undefined;

  return {
    triggerMode: isTriggerMode(triggerValue) ? triggerValue : undefined,
    customShortcut: customShortcut || undefined,
    hostMode: resolveRuntimeHostMode(argv, hostValue),
    targetUrlOverride,
    targetUrlOverrideSource: cliTargetUrl
      ? 'cli'
      : environmentTargetUrl
        ? 'environment'
        : undefined,
  };
}

export function isTriggerMode(value: unknown): value is TriggerMode {
  return typeof value === 'string' && (TRIGGER_MODES as readonly string[]).includes(value);
}

export function isHostMode(value: unknown): value is HostMode {
  return typeof value === 'string' && (HOST_MODES as readonly string[]).includes(value);
}

export function isTargetMode(value: unknown): value is TargetMode {
  return typeof value === 'string' && (TARGET_MODES as readonly string[]).includes(value);
}

function resolveRuntimeHostMode(
  argv: string[],
  explicitValue: string | undefined,
): HostMode | undefined {
  if (explicitValue !== undefined) {
    if (!isHostMode(explicitValue)) {
      throw new Error(`无效的宿主模式：${explicitValue}；可选值为 native 或 headless。`);
    }

    return explicitValue;
  }

  const useHeadless = argv.includes('--headless');
  const useNative = argv.includes('--native');
  if (useHeadless && useNative) {
    throw new Error('--headless 与 --native 不能同时使用。');
  }

  if (useHeadless) {
    return 'headless';
  }

  if (useNative) {
    return 'native';
  }

  const environmentValue = process.env.SELECT_BRIDGE_HOST_MODE;
  if (!environmentValue) {
    return undefined;
  }

  if (!isHostMode(environmentValue)) {
    throw new Error(
      `无效的 SELECT_BRIDGE_HOST_MODE：${environmentValue}；可选值为 native 或 headless。`,
    );
  }

  return environmentValue;
}

function sanitizeConfig(
  value: unknown,
  missingUiLanguage: UiLanguage = DEFAULT_CONFIG.uiLanguage,
): AppConfig {
  if (!isRecord(value)) {
    return { ...DEFAULT_CONFIG };
  }

  const migratePerformanceDefaults =
    typeof value.schemaVersion !== 'number' || value.schemaVersion < 6;
  const customShortcut =
    typeof value.customShortcut === 'string'
      ? value.customShortcut.trim()
      : DEFAULT_CONFIG.customShortcut;
  const requestedTriggerMode = isTriggerMode(value.triggerMode)
    ? value.triggerMode
    : DEFAULT_CONFIG.triggerMode;
  return {
    schemaVersion: DEFAULT_CONFIG.schemaVersion,
    enabled: getBoolean(value.enabled, DEFAULT_CONFIG.enabled),
    triggerMode:
      requestedTriggerMode === 'custom' && !customShortcut
        ? 'immediate'
        : requestedTriggerMode,
    indicatorAction:
      value.indicatorAction === 'hover' || value.indicatorAction === 'click'
        ? value.indicatorAction
        : DEFAULT_CONFIG.indicatorAction,
    maxTextLength: getPositiveInteger(value.maxTextLength, DEFAULT_CONFIG.maxTextLength),
    dedupeWindowMs: getPositiveInteger(value.dedupeWindowMs, DEFAULT_CONFIG.dedupeWindowMs),
    selectionStableMs: migratePerformanceDefaults
      ? getMigratedTiming(value.selectionStableMs, 120, DEFAULT_CONFIG.selectionStableMs)
      : getPositiveInteger(value.selectionStableMs, DEFAULT_CONFIG.selectionStableMs),
    indicatorTtlMs: getPositiveInteger(value.indicatorTtlMs, DEFAULT_CONFIG.indicatorTtlMs),
    hoverDelayMs: migratePerformanceDefaults
      ? getMigratedTiming(value.hoverDelayMs, 450, DEFAULT_CONFIG.hoverDelayMs)
      : getPositiveInteger(value.hoverDelayMs, DEFAULT_CONFIG.hoverDelayMs),
    iconSize: migratePerformanceDefaults
      ? getMigratedBoundedInteger(value.iconSize, 40, DEFAULT_CONFIG.iconSize, 24, 40)
      : getBoundedInteger(value.iconSize, DEFAULT_CONFIG.iconSize, 24, 40),
    dotSize: migratePerformanceDefaults
      ? getMigratedBoundedInteger(value.dotSize, 24, DEFAULT_CONFIG.dotSize, 12, 28)
      : getBoundedInteger(value.dotSize, DEFAULT_CONFIG.dotSize, 12, 28),
    customShortcut,
    autoStart: getBoolean(value.autoStart, DEFAULT_CONFIG.autoStart),
    uiLanguage: Object.hasOwn(value, 'uiLanguage')
      ? isUiLanguage(value.uiLanguage)
        ? value.uiLanguage
        : DEFAULT_CONFIG.uiLanguage
      : missingUiLanguage,
    ...migrateTargetConfig(value),
  };
}

export function applyEnvironment(config: Readonly<AppConfig>): AppConfig {
  const customShortcut =
    process.env.SELECT_BRIDGE_SHORTCUT?.trim() || config.customShortcut;
  const requestedTriggerMode = isTriggerMode(process.env.SELECT_BRIDGE_TRIGGER_MODE)
    ? process.env.SELECT_BRIDGE_TRIGGER_MODE
    : config.triggerMode;
  return {
    ...config,
    maxTextLength: getEnvironmentInteger(
      'SELECT_BRIDGE_MAX_LENGTH',
      config.maxTextLength,
    ),
    dedupeWindowMs: getEnvironmentInteger(
      'SELECT_BRIDGE_DEDUPE_MS',
      config.dedupeWindowMs,
    ),
    selectionStableMs: getEnvironmentInteger(
      'SELECT_BRIDGE_STABLE_MS',
      config.selectionStableMs,
    ),
    indicatorTtlMs: getEnvironmentInteger(
      'SELECT_BRIDGE_INDICATOR_TTL_MS',
      config.indicatorTtlMs,
    ),
    hoverDelayMs: getEnvironmentInteger(
      'SELECT_BRIDGE_HOVER_MS',
      config.hoverDelayMs,
    ),
    iconSize: getBoundedEnvironmentInteger(
      'SELECT_BRIDGE_ICON_SIZE',
      config.iconSize,
      24,
      40,
    ),
    dotSize: getBoundedEnvironmentInteger(
      'SELECT_BRIDGE_DOT_SIZE',
      config.dotSize,
      12,
      28,
    ),
    customShortcut,
    triggerMode:
      requestedTriggerMode === 'custom' && !customShortcut
        ? 'immediate'
        : requestedTriggerMode,
  };
}

export function isTargetUrlTemplate(value: unknown): value is string {
  if (
    typeof value !== 'string' ||
    value.length === 0 ||
    value.length > 2048 ||
    value.trim() !== value
  ) {
    return false;
  }

  if ((value.match(/\{text\}/g) ?? []).length !== 1) {
    return false;
  }

  if (/\s|[\u0000-\u001f\u007f]/.test(value)) {
    return false;
  }

  return /^[A-Za-z][A-Za-z0-9+.-]*:/.test(value);
}

function resolveConfigPath(): string {
  if (process.env.SELECT_BRIDGE_CONFIG) {
    return process.env.SELECT_BRIDGE_CONFIG;
  }

  if (process.platform === 'win32' && getDistributionMode() === 'portable') {
    return join(dirname(process.execPath), 'data', 'config.json');
  }

  const baseDirectory =
    process.platform === 'win32'
      ? process.env.APPDATA ?? join(homedir(), 'AppData', 'Roaming')
      : process.env.XDG_CONFIG_HOME ?? join(homedir(), '.config');

  return join(baseDirectory, 'select-bridge', 'config.json');
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

export function resolveConfiguredTargetTemplate(config: Readonly<AppConfig>): string {
  return config.targetMode === 'custom' && isTargetUrlTemplate(config.customTargetUrlTemplate)
    ? config.customTargetUrlTemplate
    : DEFAULT_TARGET_URL_TEMPLATE;
}

function migrateTargetConfig(
  value: Record<string, unknown>,
): Pick<AppConfig, 'targetMode' | 'customTargetUrlTemplate'> {
  if (isTargetMode(value.targetMode)) {
    const customTargetUrlTemplate = isTargetUrlTemplate(value.customTargetUrlTemplate)
      ? value.customTargetUrlTemplate
      : '';
    return {
      targetMode:
        value.targetMode === 'custom' && !customTargetUrlTemplate ? 'goldendict' : value.targetMode,
      customTargetUrlTemplate,
    };
  }

  const legacyTemplate = value.targetUrlTemplate;
  if (isTargetUrlTemplate(legacyTemplate) && legacyTemplate !== DEFAULT_TARGET_URL_TEMPLATE) {
    return { targetMode: 'custom', customTargetUrlTemplate: legacyTemplate };
  }
  return { targetMode: 'goldendict', customTargetUrlTemplate: '' };
}

function getMigratedTiming(value: unknown, previousDefault: number, currentDefault: number): number {
  return value === previousDefault ? currentDefault : getPositiveInteger(value, currentDefault);
}

function getMigratedBoundedInteger(
  value: unknown,
  previousDefault: number,
  currentDefault: number,
  minimum: number,
  maximum: number,
): number {
  return getBoundedInteger(
    value === previousDefault ? currentDefault : value,
    currentDefault,
    minimum,
    maximum,
  );
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
