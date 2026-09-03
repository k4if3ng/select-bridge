# Windows

## 功能

Windows 平台层提供：

- 系统托盘和菜单；
- 无激活悬浮图标或圆点；
- 点击和 hover 触发；
- 自定义全局快捷键及冲突检测；
- 当前用户开机启动；
- 无控制台的托盘版主程序。

这些能力由 `native/win32/` 中的纯 Node-API/Win32 模块实现，native 宿主启动时必须加载该模块。

Windows 默认使用完整 native 宿主，也可以通过 `--host=headless` 或 `--headless` 显式使用跨平台 `HeadlessHost`。native 模块加载失败时直接终止，不会隐式切换模式。

Windows headless 的通用能力和排障见 [`HEADLESS.md`](HEADLESS.md)；本文其余部分主要描述 Windows native 宿主。

## 环境

- Windows SDK
- Visual Studio/Build Tools C++ 桌面开发组件
- Python 3，且 `python` 位于环境 `PATH`
- Node.js 18+；SEA 打包要求 Node.js 24+

可在构建前确认当前环境中的 Python：

```powershell
python --version
```

## 命令

```powershell
# 编译 Win32 Node-API 模块
pnpm build:native

# 只检查原生导出，不创建窗口
pnpm check:native

# 启动托盘开发模式
pnpm start -- --host=native

# 启动 Windows 无界面模式
pnpm start -- --host=headless

# 生成 Windows x64 发布目录
pnpm build:windows

# 只生成 Portable ZIP
pnpm build:windows:portable

# 从已有基础运行目录生成 Setup EXE
pnpm build:windows:setup
```

Windows 上的 `pnpm start` 默认等价于 `--host=native`，会启动真实托盘、全局选区钩子和系统快捷键。headless 仍会启动真实全局选区钩子，但不加载 Win32 UI 模块、不创建托盘，也不使用 `RegisterHotKey`。仅在需要交互验证时运行。

## 原生结构

| 文件 | 职责 |
| --- | --- |
| `native/win32/src/addon.cc` | Node-API 参数校验、导出和宿主生命周期 |
| `native/win32/src/win32_host.*` | 消息线程、托盘、窗口、快捷键和注册表 |
| `native/win32/resources.rc` | 编译默认图标 |
| `resources/windows.manifest` | SEA 主程序权限、兼容性和系统控件视觉样式 |
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
- 点击触发和 hover 触发互斥，由托盘子菜单选择；
- 悬浮控件使用 `WS_EX_LAYERED` 和 `UpdateLayeredWindow` 的 per-pixel alpha，不交给 DWM 绘制圆角或阴影；图标模式绘制抗锯齿圆角、白色底板、2px 灰蓝边框和内缩图标，圆点模式只绘制抗锯齿蓝色圆点；
- 图标大小可选 `24/28/32/36/40 px`，默认 `32 px`；圆点大小可选 `12/16/20/24/28 px`，默认 `16 px`；
- 窗口销毁时释放对应 `HICON`。

默认图标是 `resources/icon.ico`，同时用于托盘、悬浮图标和 SEA 主 EXE。

## 快捷键

自定义快捷键至少包含一个修饰键和一个普通键。系统占用检测以实际 `RegisterHotKey` 结果为准；退出或离开 custom 模式时调用 `UnregisterHotKey`。

- 捕获窗口使用普通系统标题栏和标准 `STATIC/EDIT/BUTTON` 控件；快捷键显示区保留自定义按键捕获逻辑，但不自绘界面；
- 输入完整组合后先用独立的探测 ID 检查占用，保存时再次检查并正式注册，预览阶段不会替换当前快捷键；
- 冲突、无效组合和注册异常使用行内状态提示，不再创建额外警告弹窗；
- 裸 `Esc/Enter/Tab` 分别用于取消、保存和控件导航，带修饰键时仍作为快捷键候选；
- 移除已保存快捷键时注销系统热键；当前使用 custom 触发时同时切换到 immediate；
- 从设置入口保存只更新快捷键，从未设置的 custom 触发入口保存会同时启用 custom；
- 全新配置仍预置 `Ctrl+Alt+G`，用户主动移除后以空字符串保存，不自动恢复默认值；
- 弹窗在当前鼠标所在显示器的工作区居中，不保存窗口位置。

修改 JS/C++ 原生接口时同步检查：

- `addon.cc` 的参数数量与顺序；
- `Win32Host` 声明和实现；
- `NativeAddon` TypeScript 接口；
- `WindowsNativeHost` 调用；
- `scripts/check-native.cjs`；
- `binding.gyp` 中的源文件和系统库。

## 开机启动

开发态使用 Node 可执行文件、脚本路径和 `--silent`。发布态直接注册 `SelectBridge.exe --silent`；发布版 EXE 使用 Windows GUI 子系统，因此不会显示控制台。

```text
SelectBridge.exe --silent
```

Setup 与 Portable 共用一个开机启动值。最后启用开机启动的版本接管该值；任一版本关闭开机启动时，只删除指向自身可执行文件的值。Setup 卸载器也只清理指向安装目录的值，不影响 Portable 已登记的路径。

## 单实例

Windows 运行时使用按用户隔离的命名管道作为轻量单实例锁，不增加常驻进程或第三方运行库。Setup、Portable 和开发态使用同一个实例通道：

- 先启动的实例持有选区钩子及所选宿主；native 模式同时持有托盘；
- 后启动的实例向现有实例报告自己的分发类型，然后以成功状态退出；
- Setup 与 Portable 的配置文件继续分离，但不会同时监听选区；
- 如果现有实例正在退出，新实例会短暂重试一次；通道存在但无响应时按单实例失败关闭，避免启动第二套全局钩子。

## 发布包

`pnpm build:windows` 输出：

```text
release/
├ SelectBridge-<version>-windows-x64-portable.zip
└ SelectBridge-<version>-windows-x64-setup.exe
```

主程序使用 Node SEA，并包含应用和 `selection-hook` 的 JavaScript 代码；两个原生 `.node` 文件保留在外部。主程序清单保留 `asInvoker`，并启用 Common Controls v6 系统视觉样式。

v1.1.1 在 v1.1.0 的宿主模式解耦基础上优化运行时路径：缓存 headless 快捷键解析、避免重复隐藏指示器，并减少配置变更时的重复 native 状态同步。

v1.1.0 将宿主模式与操作系统解耦：Windows 保留默认 native 托盘宿主，同时可显式使用跨平台 headless 宿主。

v1.0.1 重点修复原生 UI 线程退出时的生命周期保护，并完善自定义快捷键捕获、移除和标准系统控件界面。

- Portable ZIP 解压后包含三个运行文件和 `portable.flag`，配置写入同目录 `data/config.json`。整个目录可移动，单独移动 EXE 不可运行；移动后需要重新启用一次开机启动，以刷新注册表中的绝对路径。
- Setup EXE 使用 Inno Setup 6，把同样三个运行文件安装到当前用户目录，创建开始菜单快捷方式和卸载项，配置继续使用 `%APPDATA%`。

基础运行目录位于 `build/windows/app/`，仅供打包流水线使用，不作为最终分发物。

## 常见问题

- 找不到 Node：检查当前 PowerShell 的 Node/FNM PATH，不要把本机绝对路径写入脚本。
- 找不到 Python：确认 `python` 可通过当前 PowerShell 的 `PATH` 直接调用。
- `.node` 无法覆盖：确认是否被运行中的 Node 进程锁定，不要终止来源不明的进程。
- 原生模块加载失败：确认 `select_bridge_win32_ui.node` 已构建，且 Node 与模块架构一致。
- 快捷键冲突：查看 `RegisterHotKey` 返回的 Windows 错误码。
- 没有 Setup EXE：安装 Inno Setup 6，或先使用 `pnpm build:windows:portable` 只生成便携包。
