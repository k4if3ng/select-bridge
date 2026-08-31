# AGENTS.md

## 项目约束

- 默认使用简体中文沟通，先说结论。
- 只修改任务范围内的文件，保留无关的用户改动。
- 代码标识符和注释使用英文；用户界面文案可以使用中文。
- 使用 `pnpm`；不要用 npm 或 yarn 改写锁文件。
- 原生构建直接使用环境 `PATH` 中的 `python`。
- 除非用户要求，不主动启动全局选区钩子、托盘窗口或 Goldendict-ng。
- 所有平台支持显式使用无界面宿主；Windows 额外提供并默认使用完整托盘宿主。不引入 Electron、WebView、Tauri 或额外常驻进程。

## 代码边界

- `src/selection/` 是 `selection-hook` 的适配层，其他模块不直接依赖第三方类型。
- `src/core/` 保持平台无关；平台能力统一通过 `PlatformHost` 提供。
- `src/targets/` 负责翻译目标和协议 URL，不负责选区状态。
- `native/` 只实现平台能力，不承载业务规则。
- Windows native 模式必须加载原生平台模块；加载失败时明确终止，不隐式降级。所有平台可显式使用 `HeadlessHost`，非 Windows 平台默认使用它。

## Git 工作流

- `main` 是唯一稳定、可发布分支；不建立长期 `dev` 分支，除非任务明确要求。
- 日常改动从最新 `main` 创建短期 `feat/*`、`fix/*`、`docs/*` 或 `build/*` 分支。
- 有代码修改任务时先确认当前分支和工作区；若当前在 `main`，先创建对应短期分支。
- 开始修改前检查 `git status`，保留用户改动；不要直接重写或强推已共享的 `main`。
- 仅在用户明确要求时直接向 `main` 提交；默认通过 Pull Request 合并。
- 每个提交只包含一个主题，使用 Conventional Commits；Pull Request 合并前可压缩临时提交。
- 发布前在 `main` 完成验证，使用不可移动的 SemVer 注释标签（例如 `v1.0.2`）。

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
