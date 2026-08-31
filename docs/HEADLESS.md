# Headless 宿主

## 定义

`headless` 表示 SelectBridge 不创建自己的托盘、设置窗口或悬浮指示器。它不是服务器模式，也不代表可以脱离桌面会话运行：全局选区监听、键盘事件和 `goldendict://` URL 处理器仍依赖当前用户的图形桌面、系统权限和 Goldendict-ng。

Headless 宿主与操作系统相互独立：Windows、macOS 和 Linux 都使用同一个 `HeadlessHost`，操作系统差异由 `selection-hook` 和通用 URL 打开器吸收。

## 启动

安装依赖后显式选择 headless：

```shell
pnpm install
pnpm start -- --host=headless
```

也可以使用快捷别名或环境变量：

```shell
pnpm start -- --headless
SELECT_BRIDGE_HOST_MODE=headless pnpm start
```

Windows PowerShell 环境变量写法：

```powershell
$env:SELECT_BRIDGE_HOST_MODE = 'headless'
pnpm start
```

宿主模式只影响本次进程，不写入 `config.json`。Windows 默认仍为 `native`，macOS/Linux 默认即为 `headless`。

## 能力

| 能力 | Headless 行为 |
| --- | --- |
| 全局选区 | 由 `selection-hook` 提供 |
| `immediate` | 支持 |
| `ctrl` / `alt` / `shift` | 支持 |
| `custom` | 通过 `key-down`/`key-up` 事件匹配 |
| `icon` / `dot` | 本次运行切换为 `immediate`，不改写配置文件 |
| 查询目标 | 通过系统 URL 处理器打开 `goldendict://` |
| 托盘和设置窗口 | 不提供 |
| 悬浮指示器 | 不提供 |
| 原生系统快捷键注册 | 不提供 |
| 快捷键占用检测 | 不提供 |
| 开机启动管理 | 不提供 |

Headless 的 `custom` 只观察 `selection-hook` 已收到的键盘事件，不调用 Windows `RegisterHotKey`，因此无法提前判断组合键是否已被系统或其他应用占用。

## 平台要求

### Windows

- 可以在未构建 `select_bridge_win32_ui.node` 的情况下显式启动 headless；
- 仍使用 Windows 单实例保护，避免 native 与 headless 同时安装两套全局选区钩子；
- 查询 URL 通过 `rundll32.exe url.dll,FileProtocolHandler` 交给系统；
- Windows native 加载失败不会自动回退，必须显式选择 `--host=headless`。

### macOS

- 为运行 SelectBridge 的终端或 Node 进程授予辅助功能权限；
- 确认 Goldendict-ng 已注册 `goldendict://`；
- 查询 URL 通过系统 `open` 命令发送。

### Linux

- X11/Wayland 下的选区与键盘事件支持范围取决于桌面环境、合成器和输入设备权限；
- 确认 `xdg-open` 或 `gio` 可用，并已注册 `goldendict://`；
- 无图形桌面会话时，全局选区监听和协议 URL 通常都不可用。

## 触发方式建议

- 希望选中即查：使用 `--trigger=immediate`；
- 希望手动确认：使用 `ctrl`、`alt` 或 `shift`；
- 希望自定义组合键：使用 `--trigger=custom --shortcut=Ctrl+Alt+G`；
- 原配置为 `icon` 或 `dot` 时无需修改文件，headless 会在内存中改用 `immediate`。

示例：

```shell
pnpm start -- --host=headless --trigger=custom --shortcut=Ctrl+Alt+G
```

## 排障

### 选区事件没有产生

先确认当前应用的文本控件能被 `selection-hook` 识别，再检查系统辅助功能、输入设备或桌面会话权限。Headless 只移除了 SelectBridge 自身的 UI，不改变底层选区捕获条件。

### 能捕获选区但没有打开 Goldendict-ng

确认 Goldendict-ng 至少启动过一次，并检查系统是否能直接打开测试 URL：

```text
goldendict://test?target=popup
```

### 自定义快捷键没有触发

组合键必须包含至少一个修饰键和一个普通键。Headless 不提供系统占用检测；如果组合键被桌面环境或当前应用拦截，请更换组合。

