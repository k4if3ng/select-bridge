# AGENTS.md

## 项目约束

- 默认使用简体中文沟通，先说结论。
- 只修改任务范围内的文件，保留无关的用户改动。
- 代码标识符和注释使用英文；用户界面文案可以使用中文。
- 使用 `pnpm`；不要用 npm 或 yarn 改写锁文件。
- 原生构建直接使用环境 `PATH` 中的 `python`。
- 除非用户要求，不主动启动全局选区钩子、托盘窗口或 Goldendict-ng。
- 默认运行方式保持 headless，不引入 Electron、WebView、Tauri 或额外常驻进程。

## 代码边界

- `src/selection/` 是 `selection-hook` 的适配层，其他模块不直接依赖第三方类型。
- `src/core/` 保持平台无关；平台能力统一通过 `PlatformHost` 提供。
- `src/targets/` 负责翻译目标和协议 URL，不负责选区状态。
- `native/` 只实现平台能力，不承载业务规则。
- headless 路径不能加载平台 UI 模块；不支持的 UI 模式应安全降级。

## 修改要求

- 保持 TypeScript `strict`、`NodeNext` 和 ESM；源码导入使用 `.js` 扩展名。
- 新增配置字段时同步默认值、清洗逻辑、平台状态和公开文档。
- 同一选区候选只能触发一次；取消、超时和触发都要清理相关状态。
- 外部文本使用 `encodeURIComponent` 构造协议 URL，禁止拼接 shell 命令。
- 新增原生源文件或系统库时同步更新对应构建配置。

## 文档

- [`README.md`](README.md)：安装、使用、配置和发布入口。
- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)：通用模块与数据流。
- [`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md)：通用开发说明。
- [`docs/WINDOWS.md`](docs/WINDOWS.md)：Windows 原生实现、构建和排障。
