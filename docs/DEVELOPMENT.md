# 开发说明

本文面向贡献者和维护者，集中说明通用开发、Headless 和 Windows 原生实现。产品使用方法见 [README](../README.zh-CN.md)、[Windows 指南](WINDOWS.md) 和 [Headless 指南](HEADLESS.md)；模块边界见[架构说明](ARCHITECTURE.md)。

## 环境

通用开发需要：

- Node.js 18+
- pnpm

Windows 原生构建还需要：

- Windows SDK
- Visual Studio 或 Build Tools 的 C++ 桌面开发组件
- Python 3，且 `python` 位于当前环境的 `PATH`
- 与目标包一致的 x64 或 ARM64 Node.js；SEA 打包需要 Node.js 24+

Goldendict-ng 只在交互验证默认查询目标时需要，TypeScript 构建和测试不依赖它。

## 安装、构建与测试

```powershell
pnpm install
pnpm build
pnpm test
```

运行应用：

```powershell
# Windows 默认 native，macOS/Linux 默认 headless
pnpm start

# 所有平台显式使用无界面宿主
pnpm start -- --host=headless

# Windows 显式使用原生托盘宿主
pnpm start -- --host=native
```

`pnpm start` 会安装真实全局选区钩子，并可能唤起 Goldendict-ng。Windows native 还会创建托盘和注册系统快捷键，仅在需要交互验证时运行。

宿主模式只影响本次进程，不写入配置。`--host` 的优先级高于 `--native` / `--headless`，命令行又高于 `SELECT_BRIDGE_HOST_MODE`。未实现的 native 宿主或 Windows native 模块加载失败都会明确终止，不会隐式降级。

## 代码边界

- `src/selection/` 只负责把 `selection-hook` 转换为内部类型；
- `src/core/` 保持平台无关；
- 平台能力通过 `PlatformHost` 提供；
- `src/targets/` 构造并打开安全的协议 URL；
- `native/` 只实现平台能力，不承载业务规则。

## 配置修改

新增或修改公开配置字段时同步检查：

- `AppConfig`、默认值和配置清洗；
- 旧配置兼容和 schema；
- 环境变量或 CLI（如果公开）；
- `PlatformState`；
- 业务与平台消费逻辑；
- README 和用户文档。

配置清洗必须兼容缺失字段、错误类型和越界值。托盘写入前重读磁盘 JSON，只合并本次修改；无效配置不得覆盖当前运行状态。

URL 模板使用 `{text}` 作为占位符。选中文字先经过 `encodeURIComponent`，再通过平台 URL 打开器传递，禁止拼接 shell 命令。

## 触发方式与查询目标

新增触发方式时同步更新 `TRIGGER_MODES`、`TriggerController`、`PlatformEvent`、平台实现和用户文档。所有入口共享同一候选，首个成功入口消费候选；取消、超时、配置切换和触发都必须清理状态。

新增查询目标时实现 `TranslationTarget`，不要把目标判断写进触发控制器。

## Headless 开发

`HeadlessHost` 可在 Windows、macOS 和 Linux 使用，不创建托盘或指示器。`immediate`、修饰键和 `custom` 触发来自 `selection-hook`；自定义快捷键只匹配收到的键盘事件，不注册或探测系统全局快捷键。

Windows headless 不加载 Win32 UI 模块，但仍使用单实例保护。平台打开 URL 的具体方式由通用系统打开器处理。

## Windows 原生开发

### 构建命令

```powershell
# 编译 Win32 Node-API 模块
pnpm build:native

# 加载并检查原生导出，不创建窗口
pnpm check:native

# 生成当前 Node.js 架构的发布目录
pnpm build:windows

# 只生成 Portable ZIP
pnpm build:windows:portable

# 从已有基础目录生成 Setup EXE
pnpm build:windows:setup
```

### 原生结构

| 文件 | 职责 |
| --- | --- |
| `native/win32/src/addon.cc` | Node-API 参数校验、导出和宿主生命周期 |
| `native/win32/src/win32_host.*` | 消息线程、托盘、窗口、快捷键和注册表 |
| `native/win32/resources.rc` | 原生模块资源入口 |
| `native/win32/strings/*.rc` | 中英文界面字符串 |
| `native/win32/binding.gyp` | 源文件、编译选项和系统库 |
| `resources/windows.manifest` | SEA 权限、兼容性和系统控件样式 |
| `src/platform/windows/windows-native-host.ts` | TypeScript 平台适配器 |

C++ 源文件使用 UTF-8，MSVC 通过 `/utf-8` 固定源字符集和执行字符集。修改原生文案后重新构建，并检查托盘、快捷键窗口、URL 窗口和状态提示。

### 线程与生命周期

```text
Node thread ── PostMessage/SendMessage ── Win32 UI thread
Node thread ◀─ napi_threadsafe_function ─ Win32 UI thread
```

Win32 UI 线程不能直接调用 JavaScript。窗口必须在创建它的线程销毁；退出时清理计时器、热键、托盘、窗口、图标和线程句柄。

指示器使用无激活置顶窗口，不抢夺焦点；点击和悬浮触发互斥。图标、圆点、窗口字体和控件布局按 DPI 处理，GDI 对象在窗口销毁时释放。

自定义快捷键以实际 `RegisterHotKey` 结果判断占用。预览阶段只探测，保存时重新注册；退出或离开 custom 模式时调用 `UnregisterHotKey`。

URL 和快捷键编辑窗口是非模态、互斥的单实例窗口。URL 保存由 TypeScript 校验并原子持久化，原生窗口只负责编辑和显示结果。

修改 TypeScript/C++ 接口时同步检查：

- `addon.cc` 的参数数量与顺序；
- `Win32Host` 声明和实现；
- `NativeAddon` TypeScript 接口；
- `WindowsNativeHost` 调用；
- `scripts/check-native.cjs`；
- `binding.gyp` 的源文件和系统库。

### 开机启动与单实例

开机启动使用当前用户的注册表启动项。更新和卸载只删除指向自身可执行文件的值，避免影响另一种分发方式。

Windows 单实例保护使用按用户隔离的命名管道。Setup、Portable 和开发实例共用实例通道，但保持各自的配置目录；退出竞态必须避免同时启动第二套全局钩子。

### Windows 打包与发布

`pnpm build:windows` 根据当前 Node.js 的 `process.arch` 生成 x64 或 ARM64 产物：

```text
release/
├ SelectBridge-<version>-windows-<arch>-portable.zip
└ SelectBridge-<version>-windows-<arch>-setup.exe
```

ARM64 构建必须使用 ARM64 Node.js、原生模块和 `selection-hook` 预编译模块；不支持 x86/ia32。

主程序使用 Node SEA，应用代码打入 EXE，原生 `.node` 文件保留在外部。清单使用 `asInvoker` 并启用 Common Controls v6。Portable 包必须整体移动；Setup 使用 Inno Setup 6。`build/windows/app/` 只供打包流程使用。

## CI 与 Pull Request

Pull Request 和推送到 `main` 的提交会触发 `.github/workflows/ci.yml`：Ubuntu 运行 TypeScript 构建和单元测试，Windows x64、ARM64 分别编译并加载检查原生模块。完整发布包只由 SemVer 标签触发的发布工作流生成。

## 文档职责

- `README.md` / `README.zh-CN.md`：产品入口、安装、快速使用和简短 FAQ；
- `docs/WINDOWS.md`：Windows 用户指南和排障；
- `docs/HEADLESS.md`：Headless 用户指南、权限和排障；
- `docs/ARCHITECTURE.md`：跨平台模块、状态和边界；
- `docs/DEVELOPMENT.md`：通用与平台开发、构建和发布；
- `CHANGELOG.md`：用户可见的版本变化。

## 常见开发问题

- 找不到 Node：检查当前 PowerShell 的 Node/FNM `PATH`；
- 找不到 Python：确认 `python` 可从当前 `PATH` 调用；
- `.node` 无法覆盖：检查是否被正在运行的开发实例锁定；
- 原生模块加载失败：确认模块已构建，且 Node 与模块架构一致；
- 快捷键注册异常：检查 `RegisterHotKey` 返回的 Windows 错误码；
- 没有 Setup EXE：安装 Inno Setup 6，或先生成 Portable 包。
