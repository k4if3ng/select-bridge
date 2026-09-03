# SelectBridge

[English](README.md) | 简体中文

在受支持的桌面应用中选中文字，并将其转发到可配置的 URL 目标。默认目标通过 `goldendict://` 打开 [GoldenDict-ng](https://github.com/xiaoyifang/goldendict-ng) 查询弹窗。

```text
选中文字 → SelectBridge → URL 模板 → 目标应用
```

SelectBridge 本身不提供词典或翻译服务。Windows 默认使用轻量原生系统托盘宿主；Windows、macOS 和 Linux 也可以运行 headless 宿主。项目不依赖 Electron、WebView 或 Tauri。

## 功能

- 通过 [`selection-hook`](https://github.com/0xfullex/selection-hook) 监听全局文本选区
- 支持立即、修饰键或自定义快捷键转发
- Windows 支持点击或悬浮触发的浮动图标和圆点
- 支持 GoldenDict-ng Popup 和自定义 URL 模板
- 提供紧凑的 Windows 托盘菜单和原生设置窗口
- Windows UI 支持 English 和简体中文
- Windows Setup 和 Portable 包均支持 x64 与 ARM64
- 对重复选区、快速连续选区和过长文本进行保护

## 安装

### Scoop

添加 `hoarfrost` bucket 并安装 SelectBridge：

```powershell
scoop bucket add hoarfrost https://github.com/k4if3ng/hoarfrost
scoop install hoarfrost/select-bridge
```

Scoop manifest 支持 Windows x64 和 ARM64 包。后续稳定版本可以通过以下命令安装：

```powershell
scoop update select-bridge
```

### Windows 发布包

从 [GitHub Releases](../../releases/latest) 下载与系统架构匹配的包：

| 包 | 说明 |
| --- | --- |
| `SelectBridge-<version>-windows-<arch>-setup.exe` | 当前用户安装，包含开始菜单和卸载入口 |
| `SelectBridge-<version>-windows-<arch>-portable.zip` | 解压后运行，可整体移动 |

`<arch>` 为 `x64` 或 `arm64`。Portable 目录中的文件必须一起保留，只移动 `SelectBridge.exe` 会导致程序无法运行。

使用默认目标前，请至少启动一次 GoldenDict-ng，让 Windows 注册 `goldendict://` 协议。

## 使用

1. 启动 SelectBridge；
2. 在受支持的应用中选中文字；
3. SelectBridge 根据当前触发方式转发文本。

默认 `immediate` 模式会自动转发；修饰键和自定义快捷键模式等待 `Ctrl`、`Alt`、`Shift` 或配置的组合键。Windows native 宿主还提供支持点击或悬浮触发的 `icon` 和 `dot` 模式。

## Windows 托盘设置

左键或右键点击托盘图标会打开同一个上下文菜单：

```text
启用选词转发
────────────
查询目标 >
触发方式 >
指示器设置 >
设置 >
────────────
退出
```

子菜单包括：

```text
查询目标 >
  GoldenDict-ng Popup
  自定义 URL
  设置 URL 模板…

触发方式 >
  立即转发
  显示图标 / 显示圆点
  按 Ctrl / Alt / Shift 转发
  自定义快捷键
  设置自定义快捷键…

指示器设置 >
  触发动作 > 点击 / 悬浮
  图标大小 >
  圆点大小 >

设置 >
  登录时自动启动
  语言 > English / 简体中文
  检查更新…
  打开配置文件
  打开配置目录
  重新加载配置
```

设置会自动保存。English（`en-US`）是默认和回退界面语言；首次运行时，简体中文系统初始化为 `zh-CN`，其他系统初始化为 `en-US`。此后使用已保存的值，不再跟随操作系统语言变化。

语言切换会立即应用到托盘菜单和当前已打开的设置窗口。支持：

- English（`en-US`）
- 简体中文（`zh-CN`）

通过“设置 → 检查更新…”将当前安装版本与 GitHub 最新稳定版进行比较。发现新版本时，SelectBridge 可以打开官方下载页面，但不会自动下载或安装更新。

## URL 模板

通过“查询目标 → 设置 URL 模板…”配置其他应用或服务。模板必须：

- 以合法 URI scheme 开头；
- 必须且只能包含一个 `{text}`；
- 不包含空白或控制字符；
- 不超过 2048 个字符。

选中文本经过 `encodeURIComponent` 编码后替换 `{text}`。保存有效模板后会立即选中“自定义 URL”；切回 GoldenDict-ng 不会删除模板。

## 配置

配置文件位置：

- 安装版：`%APPDATA%\select-bridge\config.json`
- Portable：`<便携目录>\data\config.json`
- 其他平台：`$XDG_CONFIG_HOME/select-bridge/config.json` 或 `~/.config/select-bridge/config.json`

可通过 `SELECT_BRIDGE_CONFIG` 指定其他配置路径。界面语言保存在配置 schema 10 中：

```json
{
  "schemaVersion": 10,
  "uiLanguage": "en-US"
}
```

缺少 `uiLanguage` 的旧配置会根据当前系统语言执行一次升级；非法语言值回退到 `en-US`。

## 开发

```shell
pnpm install
pnpm build
pnpm build:native
pnpm test
```

使用 `pnpm start -- --host=headless` 运行跨平台 headless 宿主。构建 Windows 原生宿主还需要与目标架构匹配的 Node.js、Windows SDK、Visual Studio C++ 工具，以及可通过 `python` 调用的 Python。

技术文档：

- [架构](docs/ARCHITECTURE.md)
- [Headless 宿主](docs/HEADLESS.md)
- [开发说明](docs/DEVELOPMENT.md)
- [Windows 实现](docs/WINDOWS.md)

## 许可证

[MIT](LICENSE)
