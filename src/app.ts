import { dirname } from 'node:path';

import {
  applyEnvironment,
  ConfigStore,
  isTargetMode,
  isTargetUrlTemplate,
  loadRuntimeOptions,
  resolveConfiguredTargetTemplate,
  type AppConfig,
  type RuntimeOptions,
} from './config.js';
import { TriggerController } from './core/trigger-controller.js';
import { isPortableShortcut } from './core/portable-shortcut.js';
import { getDistributionMode } from './distribution.js';
import {
  formatUiMessage,
  isUiLanguage,
  resolveSupportedUiLanguage,
  uiMessage,
  withErrorDetails,
  type UiLanguage,
} from './i18n.js';
import { acquireSingleInstance } from './platform/single-instance.js';
import type { PlatformEvent, PlatformHost, PlatformState } from './platform/types.js';
import { createPlatformHost } from './platform/create-platform-host.js';
import { SelectionHookAdapter } from './selection/selection-hook-adapter.js';
import { UrlTarget } from './targets/url-target.js';
import { checkForUpdates } from './updates/update-checker.js';
import { APP_VERSION } from './version.js';

type ConfigPatch = Partial<AppConfig>;

export async function runApplication(argv = process.argv.slice(2)): Promise<void> {
  const runtime = loadRuntimeOptions(argv);
  validateRuntimeTarget(runtime);
  const distribution = getDistributionMode();
  const instanceGuard = await acquireSingleInstance(distribution);
  if (!instanceGuard.isPrimary) {
    const active = instanceGuard.activeDistribution ?? 'unknown';
    console.log(`[instance] SelectBridge 已在运行（${active}）；当前 ${distribution} 实例退出。`);
    return;
  }

  const platform = await createPlatformHost(runtime.hostMode);
  const initialUiLanguage = resolveSupportedUiLanguage(platform.getSystemUiLanguage());
  const configStore = new ConfigStore(undefined, initialUiLanguage);
  let config = applyRuntimeOverrides(await configStore.load(), runtime);
  config = adaptConfigForPlatform(config, platform);

  const effectiveTargetTemplate = (): string =>
    runtime.targetUrlOverride ?? resolveConfiguredTargetTemplate(config);
  const target = new UrlTarget(effectiveTargetTemplate, (url) => platform.openExternalUrl(url));

  let hook: SelectionHookAdapter | undefined;
  let shuttingDown = false;
  let changeQueue: Promise<void> = Promise.resolve();
  let updateCheckPending: Promise<void> | undefined;
  let updateCheckAbort: AbortController | undefined;
  let controller!: TriggerController;

  const platformState = (value: AppConfig): PlatformState => toPlatformState(value, runtime);

  const applyConfig = (nextConfig: AppConfig): void => {
    config = adaptConfigForPlatform(
      applyRuntimeOverrides(applyEnvironment(nextConfig), runtime),
      platform,
    );
    controller.replaceConfig(config, false);
  };

  const queueChange = <T>(operation: () => Promise<T>): Promise<T> => {
    const queued = changeQueue.then(operation);
    changeQueue = queued.then(() => undefined, () => undefined);
    return queued;
  };

  const persistPatch = (patch: ConfigPatch): Promise<void> =>
    queueChange(async () => {
      const updated = await configStore.mergePersistent(patch);
      applyConfig(updated);
    });

  const persistControllerConfig = async (nextConfig: AppConfig): Promise<void> => {
    const patch = diffConfig(config, nextConfig);
    if (Object.keys(patch).length === 0) {
      return;
    }
    try {
      await persistPatch(patch);
    } catch (error: unknown) {
      controller.replaceConfig(config, false);
      reportConfigError(
        platform,
        config.uiLanguage,
        'saveConfigFailedTitle',
        'saveConfigFailed',
        error,
      );
    }
  };

  const shutdown = async (reason: string): Promise<void> => {
    if (shuttingDown) {
      return;
    }
    shuttingDown = true;
    console.log(`\n正在停止选词监听（${reason}）…`);
    controller.dispose();
    hook?.stop();
    hook?.cleanup();
    updateCheckAbort?.abort();
    await updateCheckPending;
    await changeQueue;
    await configStore.flush();
    await platform.stop();
    await instanceGuard.close();
  };

  controller = new TriggerController({
    config,
    platform,
    target,
    portableCustomShortcut: platform.capabilities.portableShortcut,
    toPlatformState: platformState,
    onConfigChange: (nextConfig) => {
      void persistControllerConfig(nextConfig);
    },
    onExitRequested: () => {
      void shutdown('托盘退出').finally(() => process.exit(0));
    },
  });

  await platform.start((event) => {
    void handlePlatformEvent(event).catch((error: unknown) => {
      console.error('[platform] 处理原生事件失败：', error);
    });
  });

  async function handlePlatformEvent(event: PlatformEvent): Promise<void> {
    if (event.type === 'set-target-mode') {
      if (runtime.targetUrlOverrideSource || !isTargetMode(event.value)) {
        return;
      }
      if (event.value === 'custom' && !isTargetUrlTemplate(config.customTargetUrlTemplate)) {
        platform.showError(
          uiMessage(config.uiLanguage, 'queryTargetTitle'),
          uiMessage(config.uiLanguage, 'customUrlRequired'),
        );
        return;
      }
      try {
        await persistPatch({ targetMode: event.value });
      } catch (error: unknown) {
        reportConfigError(
          platform,
          config.uiLanguage,
          'switchTargetFailedTitle',
          'switchTargetFailed',
          error,
        );
      }
      return;
    }

    if (event.type === 'save-target-url') {
      if (runtime.targetUrlOverrideSource) {
        platform.completeTargetUrlSave(
          false,
          uiMessage(config.uiLanguage, 'runtimeUrlOverride'),
        );
        return;
      }
      if (!isTargetUrlTemplate(event.value)) {
        platform.completeTargetUrlSave(
          false,
          targetValidationMessage(config.uiLanguage, event.value),
        );
        return;
      }
      try {
        await persistPatch({ customTargetUrlTemplate: event.value, targetMode: 'custom' });
        platform.completeTargetUrlSave(true);
      } catch (error: unknown) {
        platform.completeTargetUrlSave(
          false,
          withErrorDetails(config.uiLanguage, 'saveUrlFailed', errorMessage(error)),
        );
        console.error('[config] 保存自定义 URL 失败：', error);
      }
      return;
    }

    if (event.type === 'open-config-file' || event.type === 'open-config-directory') {
      try {
        await configStore.ensureExists();
        const path = event.type === 'open-config-file' ? configStore.path : dirname(configStore.path);
        if (!platform.openPath(path)) {
          platform.showError(
            uiMessage(config.uiLanguage, 'openConfigFailedTitle'),
            formatUiMessage(config.uiLanguage, 'openPathFailed', { path }),
          );
        }
      } catch (error: unknown) {
        reportConfigError(
          platform,
          config.uiLanguage,
          'openConfigFailedTitle',
          'openConfigFailed',
          error,
        );
      }
      return;
    }

    if (event.type === 'reload-config') {
      try {
        const diskConfig = await queueChange(() => configStore.readPersistent());
        applyConfig(diskConfig);
      } catch (error: unknown) {
        reportConfigError(
          platform,
          config.uiLanguage,
          'reloadConfigFailedTitle',
          'reloadConfigFailed',
          error,
        );
      }
      return;
    }

    if (event.type === 'toggle-auto-start') {
      if (!platform.capabilities.autoStart) {
        platform.showError(
          uiMessage(config.uiLanguage, 'autoStartTitle'),
          uiMessage(config.uiLanguage, 'autoStartUnsupported'),
        );
        return;
      }
      const enabled = !config.autoStart;
      if (!(await platform.setAutoStart(enabled))) {
        platform.showError(
          uiMessage(config.uiLanguage, 'autoStartTitle'),
          uiMessage(config.uiLanguage, 'autoStartUpdateFailed'),
        );
        return;
      }
      try {
        await persistPatch({ autoStart: enabled });
      } catch (error: unknown) {
        await platform.setAutoStart(!enabled);
        reportConfigError(
          platform,
          config.uiLanguage,
          'saveAutoStartFailedTitle',
          'saveAutoStartFailed',
          error,
        );
      }
      return;
    }

    if (event.type === 'set-ui-language') {
      if (!isUiLanguage(event.value) || event.value === config.uiLanguage) {
        return;
      }
      try {
        await persistPatch({ uiLanguage: event.value });
      } catch (error: unknown) {
        reportConfigError(
          platform,
          config.uiLanguage,
          'saveLanguageFailedTitle',
          'saveLanguageFailed',
          error,
        );
      }
      return;
    }

    if (event.type === 'check-for-updates') {
      if (updateCheckPending) {
        return;
      }
      updateCheckAbort = new AbortController();
      updateCheckPending = runUpdateCheck(platform, updateCheckAbort.signal).finally(() => {
        updateCheckPending = undefined;
        updateCheckAbort = undefined;
      });
      await updateCheckPending;
      return;
    }

    controller.handlePlatformEvent(event);
  }

  async function runUpdateCheck(host: PlatformHost, signal: AbortSignal): Promise<void> {
    try {
      const result = await checkForUpdates(APP_VERSION, { signal });
      if (shuttingDown) return;
      const language = config.uiLanguage;
      if (result.status === 'up-to-date') {
        host.showInfo(
          uiMessage(language, 'updateCheckTitle'),
          formatUiMessage(language, 'upToDate', { current: result.currentVersion }),
        );
        return;
      }

      const openRelease = host.confirm(
        uiMessage(language, 'updateCheckTitle'),
        formatUiMessage(language, 'updateAvailable', {
          current: result.currentVersion,
          latest: result.latestVersion,
        }),
      );
      if (openRelease && !host.openExternalUrl(result.releaseUrl)) {
        host.showError(
          uiMessage(language, 'openReleaseFailedTitle'),
          uiMessage(language, 'openReleaseFailed'),
        );
      }
    } catch (error: unknown) {
      if (shuttingDown) return;
      const language = config.uiLanguage;
      host.showError(
        uiMessage(language, 'updateCheckFailedTitle'),
        withErrorDetails(language, 'updateCheckFailed', errorMessage(error)),
      );
    }
  }

  if (config.triggerMode === 'custom' && platform.capabilities.nativeShortcut) {
    const registration = platform.registerShortcut(config.customShortcut);
    if (!registration.ok) {
      console.error(
        `[shortcut] 无法注册 ${config.customShortcut}（错误码 ${registration.errorCode}），本次运行改用 immediate。`,
      );
      config = { ...config, triggerMode: 'immediate' };
      controller.replaceConfig(config, false);
    } else if (registration.normalized !== config.customShortcut) {
      config = { ...config, customShortcut: registration.normalized };
      controller.replaceConfig(config, false);
    }
  } else if (platform.capabilities.nativeShortcut) {
    platform.registerShortcut('');
  }
  platform.updateState(platformState(config));

  hook = new SelectionHookAdapter({
    onSelection: (event) => controller.handleSelection(event),
    onKeyDown: (event) => controller.handleKeyDown(event),
    onKeyUp: (event) => controller.handleKeyUp(event),
    onError: (error) => console.error('[selection-hook]', error),
  });

  if (!hook.start()) {
    await platform.stop();
    hook.cleanup();
    throw new Error('selection-hook 启动失败');
  }

  console.log('选词转发已启动。');
  console.log(
    `运行模式：${platform.capabilities.hostMode === 'native' ? 'Windows 原生托盘' : `${process.platform} headless`}`,
  );
  console.log(`触发方式：${config.triggerMode}`);
  if (config.triggerMode === 'custom' && platform.capabilities.portableShortcut) {
    console.log(`便携快捷键：${config.customShortcut}（事件匹配，不检测系统占用）`);
  }
  console.log(`查询协议模板：${effectiveTargetTemplate()}`);
  console.log('按 Ctrl+C 退出。');

  process.once('SIGINT', () => {
    void shutdown('SIGINT').finally(() => process.exit(0));
  });
  process.once('SIGTERM', () => {
    void shutdown('SIGTERM').finally(() => process.exit(0));
  });
}

function adaptConfigForPlatform(config: AppConfig, platform: PlatformHost): AppConfig {
  if (!platform.capabilities.indicator && (config.triggerMode === 'icon' || config.triggerMode === 'dot')) {
    console.warn(`[platform] ${config.triggerMode} 模式需要桌面指示器，本次运行改用 immediate。`);
    return { ...config, triggerMode: 'immediate' };
  }
  if (
    config.triggerMode === 'custom' &&
    !platform.capabilities.nativeShortcut &&
    (!platform.capabilities.portableShortcut || !isPortableShortcut(config.customShortcut))
  ) {
    console.warn(`[shortcut] ${JSON.stringify(config.customShortcut)} 不是有效组合，本次运行改用 immediate。`);
    return { ...config, triggerMode: 'immediate' };
  }
  return config;
}

function applyRuntimeOverrides(config: AppConfig, runtime: RuntimeOptions): AppConfig {
  const updated: AppConfig = {
    ...config,
    ...(runtime.triggerMode ? { triggerMode: runtime.triggerMode } : {}),
    ...(runtime.customShortcut ? { customShortcut: runtime.customShortcut } : {}),
  };
  return updated.triggerMode === 'custom' && !updated.customShortcut
    ? { ...updated, triggerMode: 'immediate' }
    : updated;
}

function toPlatformState(config: AppConfig, runtime: RuntimeOptions): PlatformState {
  return {
    enabled: config.enabled,
    triggerMode: config.triggerMode,
    autoStart: config.autoStart,
    indicatorAction: config.indicatorAction,
    iconSize: config.iconSize,
    dotSize: config.dotSize,
    customShortcut: config.customShortcut,
    targetMode: runtime.targetUrlOverride ? 'custom' : config.targetMode,
    customTargetUrlTemplate: runtime.targetUrlOverride ?? config.customTargetUrlTemplate,
    targetUrlOverrideSource: runtime.targetUrlOverrideSource,
    uiLanguage: config.uiLanguage,
  };
}

function diffConfig(previous: AppConfig, next: AppConfig): ConfigPatch {
  const patch: ConfigPatch = {};
  for (const key of Object.keys(next) as (keyof AppConfig)[]) {
    if (key !== 'schemaVersion' && previous[key] !== next[key]) {
      Object.assign(patch, { [key]: next[key] });
    }
  }
  return patch;
}

function validateRuntimeTarget(runtime: RuntimeOptions): void {
  if (runtime.targetUrlOverride !== undefined && !isTargetUrlTemplate(runtime.targetUrlOverride)) {
    throw new Error(targetValidationMessage('en-US', runtime.targetUrlOverride));
  }
}

function targetValidationMessage(language: UiLanguage, value: string): string {
  if (value.length > 2048) {
    return uiMessage(language, 'urlTemplateTooLong');
  }
  return uiMessage(language, 'urlTemplateInvalid');
}

function errorMessage(error: unknown): string {
  return error instanceof Error ? error.message : String(error);
}

function reportConfigError(
  platform: PlatformHost,
  language: UiLanguage,
  titleKey: Parameters<typeof uiMessage>[1],
  summaryKey: Parameters<typeof uiMessage>[1],
  error: unknown,
): void {
  const title = uiMessage(language, titleKey);
  console.error(`[config] ${title}:`, error);
  platform.showError(title, withErrorDetails(language, summaryKey, errorMessage(error)));
}
