# SelectBridge

> 在任意受支持的桌面应用中选中文字，并在 [Goldendict-ng](https://github.com/xiaoyifang/goldendict-ng) 弹窗中查询。

SelectBridge 通过 [`selection-hook`](https://github.com/0xfullex/selection-hook) 监听全局文本选区，判断何时触发查询，并把选中的文字交给配置的 URL 目标；默认目标为 Goldendict-ng。**它本身不提供词典或翻译服务**。运行宿主与操作系统分开选择：所有平台都支持不创建窗口的 headless 宿主，Windows 另外提供完整系统托盘宿主。

```text
选中文字 → SelectBridge → 配置的 URL 模板 → 翻译应用
```

选区监听、触发状态机和 `goldendict://` 查询链路跨平台复用。Windows native 模式额外使用轻量 Win32 Node-API 模块提供托盘、悬浮指示器、原生全局快捷键和开机启动；Windows 的两种宿主模式都使用单实例保护。项目不依赖 Electron、WebView 或 Tauri。

## 功能一览

- 在支持的应用中监听全局文本选区；
- 选区稳定后自动查询，或通过修饰键、组合快捷键触发；Windows 还支持图标和圆点；
- Windows、macOS 和 Linux 都可用 headless 模式运行，通过 `selection-hook` 的键盘事件匹配组合快捷键；
- Windows 悬浮图标支持点击触发或悬浮触发，二者可在托盘菜单中切换；
- 过滤 Goldendict-ng 自身的选区，避免查询递归；
- 对重复文本、过长文本和快速连续选区进行保护；
- Windows 托盘菜单直接控制启用状态、触发方式、指示器大小、快捷键和开机启动；
- 提供 Setup 安装包和 Portable 便携包；
- Windows Setup、Portable 与开发态共享单实例，不会重复监听或重复查询。

## 安装

### 1. 准备 Goldendict-ng

先安装并至少启动一次 Goldendict-ng，让系统注册 `goldendict://` 协议。SelectBridge 只负责发送查询，不会替你安装词典或配置 Goldendict-ng。

### 2. Windows 发布包

从 [GitHub Releases](../../releases/latest) 下载适合你的 Windows 包。构建脚本支持 x64 和 ARM64；当前 GitHub Actions 默认发布 x64，ARM64 原生包需在 ARM64 Windows + ARM64 Node.js 环境构建：

| 包 | 适合人群 | 说明 |
| --- | --- | --- |
| `SelectBridge-<version>-windows-<arch>-setup.exe` | 大多数用户 | 当前用户安装，无需管理员权限，带开始菜单入口和卸载项；`<arch>` 为 `x64` 或 `arm64` |
| `SelectBridge-<version>-windows-<arch>-portable.zip` | 需要便携运行的用户 | 解压后直接运行，可整体移动，不写入安装目录外的程序文件 |

Portable 包必须整体移动，不能只单独移动 `SelectBridge.exe`。便携版配置保存在同目录的 `data/config.json`；安装版配置保存在 `%APPDATA%\select-bridge\config.json`。

### 3. Headless 源码运行

Windows、macOS 和 Linux 都支持源码 headless 运行，需要 Node.js 18+ 和 pnpm：

```shell
pnpm install
pnpm start -- --host=headless --trigger=immediate
```

headless 模式不创建托盘、设置窗口或悬浮指示器；查询由系统 URL 处理器转交配置的目标应用。默认目标仍是 Goldendict-ng，使用 `goldendict://` 协议。`icon`/`dot` 配置会在本次运行中切换为 `immediate`，不会覆盖配置文件。这里的 headless 仅表示 SelectBridge 自身不创建 UI，仍需要当前用户的图形桌面、全局选区权限和已注册的目标 URL 处理器。

宿主模式与操作系统能力的关系如下：

| 系统 | 默认宿主 | `--host=headless` | `--host=native` |
| --- | --- | --- | --- |
| Windows | `native` | 支持 | 支持 |
| macOS | `headless` | 支持 | 暂未提供 |
| Linux | `headless` | 支持 | 暂未提供 |

`--headless` 和 `--native` 分别是两种 `--host` 写法的快捷别名。Windows native 模块加载失败时不会自动切换模式；需要无界面运行时应显式指定 `--host=headless`。

各平台权限、触发能力和排障方式见 [`docs/HEADLESS.md`](docs/HEADLESS.md)。

### 4. 启动并查询

1. Windows 默认启动 native 托盘宿主；macOS/Linux 默认保持 headless 进程运行；任何平台都可以显式指定 `--host=headless`；
2. 在任意支持文本选区的应用中选中文字；
3. 按默认的 **immediate** 模式，SelectBridge 会自动打开 Goldendict-ng 查询弹窗。

Windows 可通过右键托盘图标暂停监听、切换触发方式、调整指示器、设置快捷键或退出程序。

## 触发方式

| 模式 | 使用方式 | 适合场景 |
| --- | --- | --- |
| `immediate`（默认） | 选区稳定后自动查询 | 希望选中即查 |
| `icon` | Windows：在选区附近显示图标，点击图标查询 | 想保留确认步骤 |
| `dot` | Windows：在选区附近显示圆点，点击圆点查询 | 希望占用更小的屏幕空间 |
| `ctrl` / `alt` / `shift` | 选区准备好后按指定修饰键查询 | 只在需要时查询 |
| `custom` | 按组合快捷键查询 | 需要自定义操作习惯 |

Windows 的图标和圆点模式都可以选择 `click` 或 `hover`：

- `click`：点击悬浮指示器后查询；
- `hover`：鼠标悬浮达到延迟后查询。

自定义快捷键至少包含一个修饰键和一个普通键，例如 `Ctrl+Alt+G` 或 `Shift+F8`。Windows native 模式使用 `RegisterHotKey` 注册并检测占用；headless 模式通过 `selection-hook` 的 `key-down`/`key-up` 事件匹配，不预约系统热键，也不检测占用。

## 配置

### Windows 托盘设置

常用选项都可以从托盘菜单修改，修改后会自动保存：

| 选项 | 默认值 | 作用 |
| --- | ---: | --- |
| 启用选词监听 | 开启 | 暂停或恢复全局选区监听 |
| 触发方式 | `immediate` | 选择自动、指示器、修饰键或自定义快捷键 |
| 指示器动作 | `click` | 图标/圆点使用点击还是悬浮触发 |
| 图标大小 | `32 px` | 可选 24/28/32/36/40 px |
| 圆点大小 | `16 px` | 可选 12/16/20/24/28 px |
| 开机启动 | 关闭 | 当前 Windows 用户登录后自动启动 |

### 配置文件位置

- 安装版：`%APPDATA%\select-bridge\config.json`
- Portable：`<便携目录>\data\config.json`
- 其他平台开发环境：`$XDG_CONFIG_HOME/select-bridge/config.json` 或 `~/.config/select-bridge/config.json`

可通过 `SELECT_BRIDGE_CONFIG` 指定自定义配置文件路径。程序会自动补全缺失字段并迁移旧版本配置。

### 命令行覆盖

开发或排障时，可以只为本次运行覆盖触发方式，不修改已保存配置：

```shell
pnpm start -- --trigger=immediate
pnpm start -- --trigger=icon
pnpm start -- --trigger=ctrl
pnpm start -- --trigger=custom --shortcut=Ctrl+Alt+G
pnpm start -- --host=headless
pnpm start -- --host=native
pnpm start -- --target-url="youdao://query?word={text}"
```

目标 URL 模板中的 `{text}` 会替换为经过 `encodeURIComponent` 编码的选中文字。默认模板为 `goldendict://{text}?target=popup`；`--target-url` 和 `SELECT_BRIDGE_TARGET_URL` 只覆盖本次运行，不修改配置文件。

高级数值配置也支持环境变量覆盖：

| 环境变量 | 作用 |
| --- | --- |
| `SELECT_BRIDGE_MAX_LENGTH` | 最大选区字符数，默认 200 |
| `SELECT_BRIDGE_DEDUPE_MS` | 重复抑制时间，默认 800ms |
| `SELECT_BRIDGE_STABLE_MS` | 选区稳定等待时间，默认 60ms |
| `SELECT_BRIDGE_INDICATOR_TTL_MS` | 指示器最长显示时间，默认 3000ms |
| `SELECT_BRIDGE_HOVER_MS` | 悬浮触发延迟，默认 350ms |
| `SELECT_BRIDGE_ICON_SIZE` | 图标边长 |
| `SELECT_BRIDGE_DOT_SIZE` | 圆点直径 |
| `SELECT_BRIDGE_SHORTCUT` | 自定义全局组合快捷键 |
| `SELECT_BRIDGE_TRIGGER_MODE` | 本次运行的触发模式 |
| `SELECT_BRIDGE_HOST_MODE` | 本次运行的宿主模式：`native` 或 `headless` |
| `SELECT_BRIDGE_NATIVE_PATH` | Windows 自定义原生 UI `.node` 模块路径 |
| `SELECT_BRIDGE_TARGET_URL` | 本次运行的目标 URL 模板，使用 `{text}` 作为选词占位符 |

## 常见问题

### 选中文字后没有反应

1. 确认 SelectBridge 仍在运行；Windows 同时确认托盘中的“启用选词监听”已打开；
2. 确认 Goldendict-ng 已启动过，并且系统能处理 `goldendict://` 协议；
3. 如果使用 `ctrl`、`alt`、`shift` 或 `custom`，按对应的触发键；
4. 选区超过 `SELECT_BRIDGE_MAX_LENGTH`（默认 200 个字符）时会被忽略。

### 悬浮指示器看不到

指示器只由 Windows native 宿主提供，会显示在选区末端附近并在默认 3 秒后消失。headless 模式请使用 `immediate`、修饰键或 `custom`。

### 快捷键无法保存

快捷键必须至少包含一个修饰键和一个普通键。若提示冲突，说明 Windows 的 `RegisterHotKey` 检测到该组合已被系统或其他程序占用，请换一个组合。

headless 模式不会显示冲突提示；它观察全局键盘事件并匹配组合键。macOS 需要为运行 SelectBridge 的终端或 Node 进程授予辅助功能权限；Linux Wayland 的支持范围取决于合成器和输入设备权限。

### Windows 启动时报原生模块错误

发布包必须使用与系统架构匹配的 Windows 文件。当前构建脚本支持 x64 和 ARM64；源码运行 native 宿主时请使用对应架构的 Node.js 并先执行 `pnpm build:native`。原生模块构建或加载失败时，程序会明确终止，不会隐式启动另一种宿主。需要无界面运行时使用 `pnpm start -- --host=headless`。

## 从源码运行

源码运行面向贡献者和需要调试的用户。Windows 发布包已经内置 Node.js，Windows 普通用户不需要安装 Node.js 或 Python。

### 通用开发环境

- Node.js 18 或更高版本；
- pnpm；

构建完整 Windows 托盘宿主还需要：

- Windows x64；
- Windows SDK；
- Visual Studio/Build Tools 的“使用 C++ 的桌面开发”；
- Python 3，并确保当前环境可以直接执行 `python`。

生成 Windows EXE 的 SEA 流程需要 Node.js 24 或更高版本，日常源码运行仍支持 Node.js 18+。

### 常用命令

```shell
# 安装依赖并编译 TypeScript
pnpm install
pnpm build

# 编译 Win32 Node-API 模块
pnpm build:native

# 仅检查原生模块是否可加载，不创建窗口
pnpm check:native

# 使用当前系统的默认宿主
pnpm start

# 所有平台都可显式启动 headless 宿主
pnpm start -- --host=headless

# 仅 Windows 提供 native 托盘宿主
pnpm start -- --host=native
```

`pnpm start` 会启动真实的全局选区钩子，并可能唤起 Goldendict-ng。Windows 默认还会启动托盘和原生系统快捷键；headless 模式不创建 UI。仅在需要交互验证时运行。

### 构建发布包

```shell
# 构建 Setup 和 Portable
pnpm build:windows

# 只构建 Portable ZIP，不要求安装 Inno Setup
pnpm build:windows:portable

# 从已有基础运行目录生成 Setup EXE
pnpm build:windows:setup
```

输出目录：

```text
release/
├ SelectBridge-<version>-windows-x64-portable.zip
└ SelectBridge-<version>-windows-x64-setup.exe
```

完整的 `build:windows` 需要 Inno Setup 6。发布脚本还会生成 `SHA256SUMS.txt` 并由 GitHub Actions 附加到 Release；详情见 [`docs/WINDOWS.md`](docs/WINDOWS.md)。

## 浏览器扩展探针

仓库还提供一个很小的 Manifest V3 扩展，用来验证浏览器通过右键菜单打开
`goldendict://` 时的外部协议确认范围。它不注入网页脚本，也不读取网站内容，只把用户
主动选中的文字交给扩展 service worker；安装和记录步骤见
[`browser-extension/README.md`](browser-extension/README.md)。该目录用于判断浏览器插件
方案的可行性，不属于当前桌面程序的发布包。

## 项目结构与设计

核心流程保持平台无关，平台能力和翻译目标通过接口隔离：

```text
selection-hook → 选区适配层 → 触发控制器 → PlatformHost → Goldendict-ng
                                                    ├ WindowsNativeHost
                                                    └ HeadlessHost
```

- `src/selection/`：把 `selection-hook` 事件转换为内部类型；
- `src/core/`：选区稳定、过滤、去重和触发决策；
- `src/platform/`：跨平台 headless 宿主及 Windows native 宿主；
- `src/targets/`：构造翻译目标协议 URL；
- `native/win32/`：纯 Node-API/Win32 实现，不承载业务规则。

更完整的模块边界、生命周期和状态机说明：

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)
- [`docs/HEADLESS.md`](docs/HEADLESS.md)
- [`docs/WINDOWS.md`](docs/WINDOWS.md)
- [`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md)

## 当前边界

- 所有平台支持 headless；Windows 额外支持 native 托盘宿主，并默认使用 native；
- 默认翻译目标为 Goldendict-ng popup，同时支持通过 URL 模板配置其他目标协议；
- headless 当前不提供托盘、悬浮指示器、开机启动管理或快捷键占用检测；
- 非 Windows 平台当前不提供应用级单实例保护；
- 暂未包含代码签名、自动更新和完整的图形化设置页；
- Windows native 模式必须加载原生模块，加载失败时不会自动降级到 headless。

## 参与贡献

欢迎提交 Issue 和 Pull Request。提交前请先阅读 [`AGENTS.md`](AGENTS.md) 中的项目约束，以及 [`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md) 中的开发说明。

建议每个 Pull Request 只聚焦一个主题，并在描述中说明影响的用户场景、验证方式和必要的回滚步骤。

## 许可证

本项目使用 MIT License（SPDX：`MIT`），详见根目录 [`LICENSE`](LICENSE) 文件。
