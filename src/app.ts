import { ConfigStore, loadRuntimeOptions, type AppConfig } from './config.js';
import { TriggerController } from './core/trigger-controller.js';
import type { PlatformEvent, PlatformHost } from './platform/types.js';
import { createPlatformHost } from './platform/create-platform-host.js';
import { SelectionHookAdapter } from './selection/selection-hook-adapter.js';
import { GoldenDictTarget } from './targets/goldendict-target.js';

export async function runApplication(argv = process.argv.slice(2)): Promise<void> {
  const runtime = loadRuntimeOptions(argv);
  const configStore = new ConfigStore();
  let config = await configStore.load();
  config = applyRuntimeOverrides(config, runtime.triggerMode);

  const platform = await createPlatformHost(runtime.mode);
  if (
    platform.kind === 'headless' &&
    (config.triggerMode === 'icon' || config.triggerMode === 'dot')
  ) {
    console.warn(`[platform] headless 不显示 ${config.triggerMode}，本次运行改用 immediate。`);
    config = { ...config, triggerMode: 'immediate' };
  }
  const target = new GoldenDictTarget();

  let hook: SelectionHookAdapter | undefined;
  let shuttingDown = false;

  const persistConfig = async (nextConfig: AppConfig): Promise<void> => {
    config = nextConfig;
    platform.updateState(toPlatformState(nextConfig));
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
  };

  const controller = new TriggerController({
    config,
    platform,
    target,
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
  platform.updateState(toPlatformState(config));

  hook = new SelectionHookAdapter({
    onSelection: (event) => controller.handleSelection(event),
    onKeyDown: (event) => controller.handleKeyDown(event),
    onError: (error) => console.error('[selection-hook]', error),
  });

  if (!hook.start()) {
    await platform.stop();
    hook.cleanup();
    throw new Error('selection-hook 启动失败');
  }

  console.log('选词转发已启动。');
  console.log(`运行模式：${platform.kind === 'tray' ? 'Windows 托盘' : 'headless'}`);
  console.log(`触发方式：${config.triggerMode}`);
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
    const enabled = !getConfig().autoStart;
    const updated = { ...getConfig(), autoStart: enabled };
    const applied = await platform.setAutoStart(enabled);

    if (!applied) {
      console.error('[startup] 无法更新开机启动设置。');
      return;
    }

    platform.updateState(toPlatformState(updated));
    await configStore.save(updated);
    controller.replaceConfig(updated);
    return;
  }

  controller.handlePlatformEvent(event);
}

function applyRuntimeOverrides(
  config: AppConfig,
  triggerMode: AppConfig['triggerMode'] | undefined,
): AppConfig {
  return triggerMode ? { ...config, triggerMode } : config;
}

function toPlatformState(config: AppConfig): Parameters<PlatformHost['updateState']>[0] {
  return {
    enabled: config.enabled,
    triggerMode: config.triggerMode,
    autoStart: config.autoStart,
  };
}
