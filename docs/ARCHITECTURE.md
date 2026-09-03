# 架构说明

## 数据流

```text
selection-hook
    │ text-selection / key-down / key-up / error
    ▼
SelectionHookAdapter
    ▼
TriggerController ◀──── ConfigStore
    │
    ├──── PlatformHost
    │
    ▼
TranslationTarget
    ▼
system URL handler ── translation application
```

选区输入、平台能力和翻译目标分别通过适配器或接口隔离，核心状态机不依赖具体操作系统和翻译软件。

## 模块

| 模块 | 职责 |
| --- | --- |
| `src/index.ts` | 最小入口和顶层错误处理 |
| `src/app.ts` | 组合配置、平台、选区监听和翻译目标 |
| `src/config.ts` | 默认配置、清洗、环境变量、CLI 和持久化 |
| `src/i18n.ts` | 支持的界面语言、系统语言归一化和 TypeScript 用户提示 |
| `src/selection/` | 将 `selection-hook` 事件转换为内部类型 |
| `src/core/trigger-controller.ts` | 稳定等待、过滤、去重和触发决策 |
| `src/core/portable-shortcut.ts` | headless 模式的组合键解析和修饰键状态匹配 |
| `src/platform/` | 跨平台 headless 宿主与 Windows native 宿主 |
| `src/targets/` | 翻译目标实现和 URL 模板 |

边界规则：

- `selection-hook` 类型只停留在 adapter；
- 平台 API 只停留在平台实现；
- 协议 URL 只由翻译目标构造；
- `TriggerController` 不识别具体操作系统或翻译程序。

## 生命周期

```text
加载运行参数和配置
  → 根据宿主模式与平台能力创建平台宿主
  → 创建触发控制器
  → 启动平台宿主
  → 启动 selection-hook
  → 监听退出信号
```

退出顺序：

```text
清理触发候选
  → hook.stop()
  → hook.cleanup()
  → platform.stop()
```

## 触发状态

```text
Idle
  → Stabilizing
  → Ready
      ├ immediate ── Consumed
      ├ shortcut ── WaitingShortcut ── Consumed
      └ indicator ── IndicatorVisible
                         ├ click/hover ── Consumed
                         └ timeout/cancel ── Idle
```

约束：

- 新选区取消旧候选；
- 稳定计时完成前不触发；
- 一个候选只消费一次；
- 触发、取消和超时都清理计时器；
- 过滤翻译程序自身的选区，避免递归；
- 同文本和同来源在去重窗口内不重复发送。

## 平台抽象

`PlatformHost` 提供生命周期、状态同步、指示器、系统协议/路径打开、原生设置结果回传、快捷键和开机启动能力，并通过 capabilities 声明实际支持项。操作系统和宿主模式是两个独立维度：`process.platform` 表示操作系统，`capabilities.hostMode` 表示当前宿主。运行参数先选择 `native` 或 `headless`，平台层再验证当前操作系统是否实现了该宿主。

- `HeadlessHost` 可在 Windows、macOS 和 Linux 使用，不创建托盘或悬浮窗口；URL 交给通用系统打开器，自定义组合键由 `selection-hook` 的 `key-down`/`key-up` 事件匹配。
- `WindowsNativeHost` 只在 Windows 提供，保留托盘、指示器、`RegisterHotKey` 冲突检测和开机启动。
- Windows 默认选择 `native`，macOS/Linux 默认选择 `headless`；`--host`、`--headless`、`--native` 或 `SELECT_BRIDGE_HOST_MODE` 可以覆盖默认值。
- 显式请求未实现的 `native` 宿主会直接报错。Windows native 模块加载失败时也明确终止，不做隐式模式切换；用户可显式选择 `--host=headless`。
- HeadlessHost 遇到 `icon`/`dot` 配置时只为本次运行切换到 `immediate`，不覆盖持久化配置。

具体宿主和平台细节放在对应文档中：跨平台无界面运行见 [`HEADLESS.md`](HEADLESS.md)，Windows 原生线程、系统 API、构建和发布见 [`WINDOWS.md`](WINDOWS.md)。

## 翻译目标

URL 目标根据配置模板生成查询地址。模板中的 `{text}` 会被替换为经过 `encodeURIComponent` 编码的选中文字：

```text
goldendict://{text}?target=popup
```

配置 schema 10 用 `targetMode` 保存目标预设、用 `customTargetUrlTemplate` 保存自定义模板，并用 `uiLanguage` 保存 `en-US` 或 `zh-CN`。旧配置缺少语言时按系统首选 UI 语言解析一次并持久化；非法值回退到 `en-US`。`UrlTarget` 在每次查询时通过 getter 解析有效模板，因此托盘保存或重新加载后无需重建目标对象。有效目标值优先级为 CLI、环境变量、配置、默认 GoldenDict-ng 模板。

默认模板保持 GoldenDict-ng popup 的兼容行为。系统 URL 打开器使用参数数组调用，不经过 shell 字符串拼接。新增翻译软件时可以配置新的 URL 模板或实现新的 `TranslationTarget`，不要把目标判断写进触发控制器。

## 界面本地化

Win32 控件和菜单文案使用 `native/win32/strings/<language>.rc` 中带语言标记的 `STRINGTABLE`，按资源 ID 和明确语言加载，缺失词条回退到英文。TypeScript 产生的用户错误信息集中在 `src/i18n.ts`。协议占位符、配置字段、产品名和内部错误码不参与翻译。

语言切换先由 TypeScript 原子持久化，再通过 `PlatformState` 投递到 Win32 UI 线程。托盘菜单在下次打开时自然重建；已打开的设置窗口只刷新标题和文案，保留输入、快捷键捕获状态、焦点以及 URL 保存状态。headless 宿主保留配置字段但不显示界面。
