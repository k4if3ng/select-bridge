# 架构说明

## 数据流

```text
selection-hook
    │ text-selection / key-down / error
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
| `src/selection/` | 将 `selection-hook` 事件转换为内部类型 |
| `src/core/trigger-controller.ts` | 稳定等待、过滤、去重和触发决策 |
| `src/platform/` | headless 与平台能力抽象 |
| `src/targets/` | 翻译目标实现 |

边界规则：

- `selection-hook` 类型只停留在 adapter；
- 平台 API 只停留在平台实现；
- 协议 URL 只由翻译目标构造；
- `TriggerController` 不识别具体操作系统或翻译程序。

## 生命周期

```text
加载配置
  → 创建平台宿主和触发控制器
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

`PlatformHost` 提供生命周期、状态同步、指示器、快捷键和开机启动等可选能力。`HeadlessHost` 不创建 UI、系统热键或启动项。平台实现加载失败时，应用回退到 headless。

具体平台的线程、系统 API、构建和发布方式放在对应平台文档中。Windows 见 [`WINDOWS.md`](WINDOWS.md)。

## 翻译目标

Goldendict-ng 目标生成：

```text
goldendict://${encodeURIComponent(text)}?target=popup
```

系统 URL 打开器使用参数数组调用，不经过 shell 字符串拼接。新增翻译软件时实现新的 `TranslationTarget`，不要把目标判断写进触发控制器。
