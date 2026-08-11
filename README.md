# Selection Forward

通过 [`selection-hook`](https://github.com/0xfullex/selection-hook) 监听全局文本选区，并将文本发送到 [`Goldendict-ng`](https://github.com/xiaoyifang/goldendict-ng)：

```text
goldendict://<URL 编码的选词>?target=popup
```

项目保留两种启动方式：

- **headless**：默认方式，不加载 GUI、托盘或 Win32 UI 模块，保持最低常驻占用。
- **Windows 托盘**：按需动态加载轻量 Win32 Node-API 模块，提供托盘、悬浮图标/小圆点和开机启动。

项目不使用 Electron、WebView 或 Python 运行时。Python 只在构建原生模块时由 `uv` 提供给 `node-gyp`。

## 前置条件

通用运行条件：

- Node.js 18 或更高版本；
- pnpm；
- 已安装并至少启动过一次 Goldendict-ng，使系统注册 `goldendict://`。

构建 Windows 托盘模块还需要：

- Windows SDK；
- Visual Studio/Build Tools 的“使用 C++ 的桌面开发”；
- [`uv`](https://docs.astral.sh/uv/)，用于向 `node-gyp` 提供 Python。

## 安装

```shell
pnpm install
pnpm build
```

## Headless 启动

```shell
pnpm start
```

这是默认、占用最低的运行方式，只启动 Node、`selection-hook` 和触发状态机。默认触发方式为 `immediate`，选择文字稳定 120ms 后立即发送到 Goldendict-ng。

也可以直接指定快捷键触发：

```shell
pnpm start -- --trigger=ctrl
pnpm start -- --trigger=alt
pnpm start -- --trigger=shift
```

如果配置文件中保存的是 `icon` 或 `dot`，headless 模式会在当前运行中自动降级为 `immediate`，不会因为没有 GUI 而丢失查询。

## Windows 托盘启动

先构建原生模块：

```powershell
pnpm build:native
```

构建脚本通过 `uv python find` 获取 Python，不依赖系统 Python 配置。

然后启动托盘模式：

```shell
pnpm start:tray
```

托盘菜单目前支持：

- 启用/暂停划词翻译；
- 立即翻译；
- 显示图标；
- 显示小圆点；
- 按 Ctrl、Alt 或 Shift 触发；
- 开机启动；
- 退出。

原生模块没有编译或加载失败时，程序会输出警告并安全回退到 headless。

## 触发状态机

```text
检测到选区
    ↓
归一化、来源过滤、长度保护
    ↓
等待 120ms，确认选区稳定
    ↓
┌ immediate ───────────────→ 查询
├ ctrl / alt / shift ──────→ 等待修饰键
└ icon / dot ──────────────→ 显示原生悬浮控件
                                   ├ 点击
                                   └ 悬浮达到延迟
                                         ↓
                                       查询
```

同一候选选区只会被消费一次；新的选区会取消旧候选。图标默认 3 秒后消失，重复文本在 800ms 内不会连续发送。

## 配置

托盘设置会持久化为 JSON。默认位置为：

- Windows：`%APPDATA%\selection-forward\config.json`
- 其他平台：`$XDG_CONFIG_HOME/selection-forward/config.json` 或 `~/.config/selection-forward/config.json`

可以通过 `SELECTION_FORWARD_CONFIG` 指定其他位置。

主要配置字段：

| 字段 | 默认值 | 作用 |
| --- | ---: | --- |
| `enabled` | `true` | 是否启用选词监听 |
| `triggerMode` | `immediate` | `immediate/icon/dot/ctrl/alt/shift` |
| `indicatorAction` | `click` | `click` 或 `hover`；hover 模式仍可点击 |
| `maxTextLength` | `200` | 最大选区字符数 |
| `dedupeWindowMs` | `800` | 重复抑制时间 |
| `selectionStableMs` | `120` | 选区稳定时间 |
| `indicatorTtlMs` | `3000` | 悬浮控件显示期限 |
| `hoverDelayMs` | `450` | 悬浮触发延迟 |
| `autoStart` | `false` | 当前用户开机启动 |

环境变量仍可覆盖常用数值：

| 环境变量 | 作用 |
| --- | --- |
| `SELECTION_FORWARD_MAX_LENGTH` | 最大选区字符数 |
| `SELECTION_FORWARD_DEDUPE_MS` | 重复抑制时间 |
| `SELECTION_FORWARD_STABLE_MS` | 选区稳定时间 |
| `SELECTION_FORWARD_INDICATOR_TTL_MS` | 悬浮控件显示期限 |
| `SELECTION_FORWARD_HOVER_MS` | 悬浮触发延迟 |
| `SELECTION_FORWARD_TRIGGER_MODE` | 触发模式 |
| `SELECTION_FORWARD_NATIVE_PATH` | 自定义 `.node` 模块路径 |

## 原生模块边界

Windows 原生模块位于 `native/win32/`，使用纯 Node-API 和 Win32 API：

- 独立 Win32 消息线程；
- `Shell_NotifyIconW` 托盘；
- `WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST` 悬浮窗口；
- `napi_threadsafe_function` 将点击和托盘事件返回 Node；
- `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` 开机启动；
- 每显示器 DPI 感知和工作区边界限制。

headless 入口不会导入这个模块。原生模块的构建结果位于：

```text
native/win32/build/Release/selection_forward_win32.node
```

仅检查模块能否加载以及导出是否完整，不创建任何窗口：

```shell
pnpm check:native
```

## 当前阶段边界

这一阶段已经完成核心状态机、托盘和悬浮控件的原生链路。托盘中的“设置…”目前只输出配置位置；完整的原生设置窗口、应用图标资源、单实例和安装器将在后续阶段实现。
