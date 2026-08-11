# 开发说明

## 环境

- Node.js 18+
- pnpm
- 已注册 `goldendict://` 的 Goldendict-ng

安装与编译：

```powershell
pnpm install
pnpm build
```

最低占用模式：

```powershell
pnpm start
```

`pnpm start` 会安装真实全局选区钩子，并可能唤起 Goldendict-ng。仅在需要交互验证时运行。

平台专用工具链和命令放在对应平台文档中。Windows 见 [`WINDOWS.md`](WINDOWS.md)。

## 配置

新增配置字段时同步更新：

- `AppConfig` 和 `DEFAULT_CONFIG`
- 配置清洗
- 环境变量或 CLI（如果公开）
- `PlatformState`
- 业务或平台消费逻辑
- README 配置表

配置清洗必须兼容缺失字段、错误类型和越界数值。

## 触发方式

新增触发方式时同步更新：

- `TRIGGER_MODES`
- headless 降级规则
- `TriggerController`
- `PlatformEvent`
- 相关平台实现
- 使用文档

所有触发入口共享同一候选。首个成功入口消费候选，避免快捷键、点击和 hover 重复发送。

## 翻译目标

新增翻译软件时实现 `TranslationTarget`，并在应用组合层选择目标。目标负责构造安全的协议 URL，核心触发逻辑不包含目标特定判断。

## 文档

- `README.md`：安装、使用、配置和发布入口。
- `docs/ARCHITECTURE.md`：通用模块、状态和边界。
- 平台文档：平台 API、工具链、构建、发布和排障。
