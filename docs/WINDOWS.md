# Windows

## 功能

Windows 平台层提供：

- 系统托盘和菜单；
- 无激活悬浮图标或圆点；
- 点击和 hover 触发；
- 自定义全局快捷键及冲突检测；
- 当前用户开机启动；
- 无控制台的静默托盘启动器。

这些能力由 `native/win32/` 中的纯 Node-API/Win32 模块实现。headless 模式不会加载该模块。

## 环境

- Windows SDK
- Visual Studio/Build Tools C++ 桌面开发组件
- `uv`
- Node.js 18+；SEA 打包要求 Node.js 24+

原生构建所需 Python 通过 `uv` 获取：

```powershell
uv python find
```

不要直接调用 `python` 或 `python3`。

## 命令

```powershell
# 编译 Win32 Node-API 模块
pnpm build:native

# 只检查原生导出，不创建窗口
pnpm check:native

# 启动托盘开发模式
pnpm start:tray

# 生成 Windows x64 发布目录
pnpm build:windows
```

`start:tray` 会启动真实托盘、全局选区钩子和系统快捷键，仅在需要交互验证时运行。

## 原生结构

| 文件 | 职责 |
| --- | --- |
| `native/win32/src/addon.cc` | Node-API 参数校验、导出和宿主生命周期 |
| `native/win32/src/win32_host.*` | 消息线程、托盘、窗口、快捷键和注册表 |
| `native/win32/src/tray_launcher.cc` | 静默启动发布版主程序 |
| `native/win32/resources.rc` | 编译默认图标 |
| `native/win32/binding.gyp` | node-gyp 源文件、编译选项和系统库 |
| `src/platform/windows/windows-native-host.ts` | TypeScript 平台适配器 |

线程关系：

```text
Node thread ── PostMessage/SendMessage ── Win32 UI thread
Node thread ◀─ napi_threadsafe_function ─ Win32 UI thread
```

Win32 UI 线程不能直接调用 JS。窗口必须在创建它的线程销毁；退出时清理计时器、热键、托盘、窗口、图标和线程句柄。

## 指示器

- 使用 `WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST`，不抢夺焦点；
- 坐标优先使用选区末端，其次鼠标末端，最后当前鼠标位置；
- 点击始终可用，hover 由鼠标跟踪和计时器触发；
- 自定义 `.ico` 加载失败时回退到内置资源；
- 图标替换和窗口销毁时释放对应 `HICON`。

默认图标是 `resources/icon.ico`，同时用于托盘、悬浮图标、静默启动器和 SEA 主 EXE。

## 快捷键

自定义快捷键至少包含一个修饰键和一个普通键。系统占用检测以实际 `RegisterHotKey` 结果为准；退出或离开 custom 模式时调用 `UnregisterHotKey`。

修改 JS/C++ 原生接口时同步检查：

- `addon.cc` 的参数数量与顺序；
- `Win32Host` 声明和实现；
- `NativeAddon` TypeScript 接口；
- `WindowsNativeHost` 调用；
- `scripts/check-native.cjs`；
- `binding.gyp` 中的源文件和系统库。

## 开机启动

开发态使用 Node 可执行文件、脚本路径和 `--silent`。发布态优先注册同目录的 `SelectionForwardTray.exe`。启动器使用 `CREATE_NO_WINDOW` 启动：

```text
SelectionForward.exe --tray --silent
```

随后启动器立即退出，不增加常驻进程。

## 发布包

`pnpm build:windows` 输出：

```text
release/windows-x64/
├ SelectionForward.exe
├ SelectionForwardTray.exe
├ selection_forward_win32_ui.node
└ node_modules/
   ├ node-gyp-build/
   └ selection-hook/
```

主程序使用 Node SEA，但原生 `.node` 文件保留在外部。发布时复制整个目录，不能只复制主 EXE。

## 常见问题

- 找不到 Node：检查当前 PowerShell 的 Node/FNM PATH，不要把本机绝对路径写入脚本。
- uv 缓存无权限：为原生构建提供最小必要权限，不要把 Python 复制进仓库。
- `.node` 无法覆盖：确认是否被运行中的 Node 进程锁定，不要终止来源不明的进程。
- 托盘回退 headless：确认 `selection_forward_win32_ui.node` 已构建，且 Node 与模块架构一致。
- 快捷键冲突：查看 `RegisterHotKey` 返回的 Windows 错误码。
