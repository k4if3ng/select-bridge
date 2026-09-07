# Headless 指南

Headless 宿主不创建托盘、设置窗口或悬浮指示器，适合希望从终端启动 SelectBridge 的用户。它仍需要当前用户的图形桌面、选区权限和可用的 URL 处理器，不是服务器模式。

Windows、macOS 和 Linux 都可以使用 Headless；Windows 默认仍使用原生托盘宿主，macOS 和 Linux 默认使用 Headless。

## 启动

```shell
pnpm install
pnpm start -- --host=headless
```

也可以使用快捷参数或环境变量：

```shell
pnpm start -- --headless
SELECT_BRIDGE_HOST_MODE=headless pnpm start
```

Windows PowerShell：

```powershell
$env:SELECT_BRIDGE_HOST_MODE = 'headless'
pnpm start
```

宿主选择只影响本次进程，不写入 `config.json`。

## 能力与限制

| 能力 | Headless 行为 |
| --- | --- |
| 全局文本选区 | 支持，取决于系统权限和当前应用 |
| 立即转发 | 支持 |
| Ctrl / Alt / Shift | 支持，单独按下并释放时触发 |
| 自定义快捷键 | 支持，但不检测系统占用 |
| 图标 / 圆点 | 不支持，本次运行改用立即转发 |
| 自定义查询目标 | 支持 |
| 托盘和设置窗口 | 不提供 |
| 开机启动管理 | 不提供 |

Headless 自定义快捷键只能观察实际收到的键盘事件。如果组合键被桌面环境或当前应用拦截，SelectBridge 可能无法触发。

## 平台提示

### Windows

- 可以在不使用原生托盘模块的情况下显式启动 Headless；
- native 与 Headless 仍受同一套单实例保护，不能同时监听选区；
- Windows native 启动失败时不会自动切换到 Headless，需要显式指定 `--host=headless`。

### macOS

- 为运行 SelectBridge 的终端或 Node.js 进程授予辅助功能权限；
- 确认目标应用或 `goldendict://` URL 处理器已经注册。

### Linux

- X11 和 Wayland 的支持取决于桌面环境、合成器和输入权限；
- 确认系统可以打开配置的查询 URL；
- 没有图形桌面会话时，通常无法监听全局选区或打开查询窗口。

## 触发方式建议

- 希望选中即查：使用 `--trigger=immediate`；
- 希望手动确认：使用 `ctrl`、`alt` 或 `shift`，选中后单独按下并释放；
- 希望使用组合键：使用 `--trigger=custom --shortcut=Ctrl+Alt+G`。

示例：

```shell
pnpm start -- --host=headless --trigger=custom --shortcut=Ctrl+Alt+G
```

## 常见问题

### 没有检测到选区

确认当前应用允许系统无障碍接口读取选中文字，并检查辅助功能、桌面会话和输入设备权限。不同应用和桌面环境的支持范围可能不同。

### 检测到选区，但没有打开查询目标

确认 GoldenDict-ng 或自定义 URL 的处理程序已经安装，并检查系统能否直接打开：

```text
goldendict://test?target=popup
```

### 自定义快捷键没有触发

组合键必须包含至少一个修饰键和一个普通键。Headless 不检测快捷键占用；如果组合被桌面环境或当前应用拦截，请更换组合。
