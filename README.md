# 选词转发到 Goldendict-ng

这个小程序通过 [`selection-hook`](https://github.com/0xfullex/selection-hook) 监听全局文本选区，并使用 [`Goldendict-ng`](https://github.com/xiaoyifang/goldendict-ng) 的 URL scheme 打开弹窗查词：

```text
goldendict://<经过 URL 编码的选词>?target=popup
```

例如，选中的 `hello world` 会转换为：

```text
goldendict://hello%20world?target=popup
```

程序会把地址交给当前系统的默认 URL 处理器。Goldendict-ng 已在运行时，请求会转发给现有实例。

## 前置条件

- Node.js 18 或更高版本。
- pnpm。
- 已安装并能正常使用 Goldendict-ng。
- 至少启动过一次 Goldendict-ng，使系统注册 `goldendict://` 协议。

## 安装与启动

```shell
pnpm install
pnpm start
```

`pnpm start` 会先编译 TypeScript，再启动常驻监听。此时在其他应用中选择单词或短语，Goldendict-ng 应显示相应的弹窗释义。按 `Ctrl+C` 停止监听。

只编译、不启动监听：

```shell
pnpm build
```

编译结果位于 `dist/`。

## 可选调整

默认只转发长度不超过 200 个字符的选区，并在 800 ms 内忽略相同文本的重复事件。这能避免误选大段内容或底层 API 重复上报时连续触发查询。

| 环境变量 | 默认值 | 作用 |
| --- | ---: | --- |
| `SELECTION_FORWARD_MAX_LENGTH` | `200` | 允许转发的最大字符数 |
| `SELECTION_FORWARD_DEDUPE_MS` | `800` | 相同文本的重复事件抑制时间，单位为毫秒 |

在启动 `pnpm start` 前，使用当前终端支持的方式设置这些环境变量即可。

## 工作链路

```text
应用中的文本选区
        ↓
selection-hook 的平台原生选区检测
        ↓
空白归一化、长度保护、短时间去重
        ↓
goldendict://<URL 编码的选词>?target=popup
        ↓
系统默认 URL 处理器
        ↓
Goldendict-ng 弹窗显示释义
```

## 可能的问题

- **未出现查词窗口**：在浏览器地址栏中尝试打开 `goldendict://test?target=popup`。若仍无反应，请重新启动或更新 Goldendict-ng，让它重新注册 URL scheme。
- **个别软件不触发**：某些自定义控件、隔离环境或权限级别不同的应用可能不会向系统暴露文本选区。
- **日志提示无法调用系统 URL 处理器**：确认当前会话具有桌面环境，并且可以通过系统默认方式打开外部链接。
