import { ConfigStore, loadRuntimeOptions, type AppConfig } from './config.js';
import { TriggerController } from './core/trigger-controller.js';
import { isPortableShortcut } from './core/portable-shortcut.js';
import { getDistributionMode } from './distribution.js';
import { acquireSingleInstance } from './platform/single-instance.js';
import type { PlatformEvent, PlatformHost } from './platform/types.js';
import { createPlatformHost } from './platform/create-platform-host.js';
import { SelectionHookAdapter } from './selection/selection-hook-adapter.js';
import { GoldenDictTarget } from './targets/goldendict-target.js';

export async function runApplication(argv = process.argv.slice(2)): Promise<void> {
  const runtime = loadRuntimeOptions(argv);
  const distribution = getDistributionMode();
  const instanceGuard = await acquireSingleInstance(distribution);
  if (!instanceGuard.isPrimary) {
    const active = instanceGuard.activeDistribution ?? 'unknown';
    console.log(
      `[instance] SelectBridge 已在运行（${active}）；当前 ${distribution} 实例退出。`,
    );
    return;
  }

  const configStore = new ConfigStore();
  let config = await configStore.load();
  config = applyRuntimeOverrides(config, runtime.triggerMode, runtime.customShortcut);

  const platform = await createPlatformHost(runtime.hostMode);
  config = adaptConfigForPlatform(config, platform);
  const target = new GoldenDictTarget((url) => platform.openExternalUrl(url));

  let hook: SelectionHookAdapter | undefined;
  let shuttingDown = false;

  const persistConfig = async (nextConfig: AppConfig): Promise<void> => {
    config = nextConfig;
    await configStore.save(nextConfig);
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
    await platform.stop();
    await instanceGuard.close();
  };

  const controller = new TriggerController({
    config,
    platform,
    target,
    portableCustomShortcut: platform.capabilities.portableShortcut,
    onConfigChange: (nextConfig) => {
      void persistConfig(nextConfig).catch((error: unknown) => {
        console.error('[config] 保存配置失败：', error);
      });
    },
    onExitRequested: () => {
      void shutdown('托盘退出').finally(() => process.exit(0));
    },
  });

  await platform.start((event) => {
    void handlePlatformEvent(event, controller, platform, configStore, () => config).catch(
      (error: unknown) => {
        console.error('[platform] 处理原生事件失败：', error);
      },
    );
  });

  if (config.triggerMode === 'custom' && platform.capabilities.nativeShortcut) {
    const registration = platform.registerShortcut(config.customShortcut);
    if (!registration.ok) {
      console.error(
        `[shortcut] 无法注册 ${config.customShortcut}（错误码 ${registration.errorCode}），本次运行改用 immediate。`,
      );
      config = { ...config, triggerMode: 'immediate' };
      controller.replaceConfig(config);
    } else if (registration.normalized !== config.customShortcut) {
      config = { ...config, customShortcut: registration.normalized };
      controller.replaceConfig(config);
    }
  } else if (platform.capabilities.nativeShortcut) {
    platform.registerShortcut('');
  }
  platform.updateState(toPlatformState(config));

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
  console.log('查询协议：goldendict://<选词>?target=popup');
  console.log('按 Ctrl+C 退出。');

  process.once('SIGINT', () => {
    void shutdown('SIGINT').finally(() => process.exit(0));
  });
  process.once('SIGTERM', () => {
    void shutdown('SIGTERM').finally(() => process.exit(0));
  });
}

async function handlePlatformEvent(
  event: PlatformEvent,
  controller: TriggerController,
  platform: PlatformHost,
  configStore: ConfigStore,
  getConfig: () => AppConfig,
): Promise<void> {
  if (event.type === 'open-settings') {
    console.log(`[settings] 配置文件：${configStore.path}`);
    return;
  }

  if (event.type === 'toggle-auto-start') {
    if (!platform.capabilities.autoStart) {
      console.error('[startup] 当前无界面平台宿主不支持管理开机启动。');
      return;
    }
    const enabled = !getConfig().autoStart;
    const updated = { ...getConfig(), autoStart: enabled };
    const applied = await platform.setAutoStart(enabled);

    if (!applied) {
      console.error('[startup] 无法更新开机启动设置。');
      return;
    }

    await configStore.save(updated);
    controller.replaceConfig(updated);
    return;
  }

  controller.handlePlatformEvent(event);
}

function adaptConfigForPlatform(config: AppConfig, platform: PlatformHost): AppConfig {
  if (
    !platform.capabilities.indicator &&
    (config.triggerMode === 'icon' || config.triggerMode === 'dot')
  ) {
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

function applyRuntimeOverrides(
  config: AppConfig,
  triggerMode: AppConfig['triggerMode'] | undefined,
  customShortcut: string | undefined,
): AppConfig {
  const updated: AppConfig = {
    ...config,
    ...(triggerMode ? { triggerMode } : {}),
    ...(customShortcut ? { customShortcut } : {}),
  };
  return updated.triggerMode === 'custom' && !updated.customShortcut
    ? { ...updated, triggerMode: 'immediate' }
    : updated;
}

function toPlatformState(config: AppConfig): Parameters<PlatformHost['updateState']>[0] {
  return {
    enabled: config.enabled,
    triggerMode: config.triggerMode,
    autoStart: config.autoStart,
    indicatorAction: config.indicatorAction,
    iconSize: config.iconSize,
    dotSize: config.dotSize,
    customShortcut: config.customShortcut,
  };
}
