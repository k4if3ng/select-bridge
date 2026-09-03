# 开发说明

## 环境

- Node.js 18+
- pnpm
- 已注册 `goldendict://` 的 Goldendict-ng

安装与编译：

```powershell
pnpm install
pnpm build

# 运行 TypeScript 单元测试
pnpm test
```

运行：

```powershell
# 使用当前系统默认宿主：Windows 为 native，macOS/Linux 为 headless
pnpm start

# 所有平台显式使用无界面宿主
pnpm start -- --host=headless

# Windows 显式使用原生托盘宿主
pnpm start -- --host=native
```

`pnpm start` 会安装真实全局选区钩子，并可能唤起 Goldendict-ng。Windows 默认同时创建托盘和原生快捷键；仅在需要交互验证时运行。

headless 宿主在 Windows、macOS 和 Linux 上行为一致：不创建托盘或指示器，`immediate`、修饰键和 `custom` 触发由 `selection-hook` 提供，查询继续通过配置的 URL 模板发送，默认模板为 `goldendict://{text}?target=popup`。无界面模式的自定义快捷键只做事件匹配，不提供系统占用检测。

宿主模式不写入持久化配置。CLI `--host=native|headless` 的优先级高于别名 `--native`/`--headless`，CLI 又高于 `SELECT_BRIDGE_HOST_MODE`。非 Windows 平台显式请求 native 会立即报错；Windows native 加载失败也不会自动回退。

宿主和平台专用工具链放在对应文档中：headless 见 [`HEADLESS.md`](HEADLESS.md)，Windows native 见 [`WINDOWS.md`](WINDOWS.md)。

## 配置

新增配置字段时同步更新：

- `AppConfig` 和 `DEFAULT_CONFIG`
- 配置清洗
- 环境变量或 CLI（如果公开）
- `PlatformState`
- 业务或平台消费逻辑
- README 配置表

配置清洗必须兼容缺失字段、错误类型和越界数值。

目标 URL 模板使用 `{text}` 作为选词占位符；程序会先执行 `encodeURIComponent`，再通过平台 URL 打开器传递，不经过 shell 字符串拼接。

## 触发方式

新增触发方式时同步更新：

- `TRIGGER_MODES`
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
- `docs/HEADLESS.md`：跨平台无界面宿主、权限和排障。
- 平台文档：平台 API、工具链、构建、发布和排障。
