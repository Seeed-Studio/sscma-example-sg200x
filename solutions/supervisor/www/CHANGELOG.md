# Changelog (0.2.2 → feature/sensecraft-classify-dashboard)

## English

### Added
- `action=train` flow: fetch model info via train API v2, download model, upload to device, create cloud app, deploy dashboard flow, auto‑navigate to dashboard when ready.
- Default dashboard flow moved to `src/utils/flowDefaults.ts` and re‑exported via `src/utils/index.ts`.
- Train flow enhancements: device type guard, dashboard readiness polling, upload progress, slow‑upload warning, cancel upload (abort).
- `action=model` flow selection: use `DefaultFlowDataWithDashboard` when `task=classify` and `model_format=cvimodel`, otherwise use default flow; update `model` node fields before deploy.
- App list UI fixes: overflow handling and tooltip for long names.
- One‑click deploy script `scripts/deploy.sh`.
- Train API v2 model info wrappers and types.

### Changed
- `action=train` now creates a cloud app (name `classify_<model>`), deploys flow, and redirects in‑tab to dashboard after readiness check.
- `action=model` logs applyModel response and conditionally auto‑opens dashboard.
- Redirect handling improved: `redirect_url` encoded; session action cached/cleaned to avoid accidental re‑runs.
- `sensecraftRequest` refresh now retries on `code=401`.

### Fixed
- App name overflow no longer hides edit/delete buttons.
- Upload flow provides progress, warning, and cancel option.
- Dashboard jump uses current tab with readiness gating.

## 中文

### 新增
- `action=train` 全流程：训练平台 v2 取模型信息 → 下载 → 上传设备 → 创建云端应用 → 部署 Dashboard Flow → 就绪后自动跳转。
- 默认 Dashboard Flow 拆分至 `src/utils/flowDefaults.ts`，由 `src/utils/index.ts` 重新导出。
- 训练流程增强：设备类型校验、Dashboard 就绪轮询、上传进度提示、慢上传提醒、支持取消上传。
- `action=model` 按 `task=classify` 且 `model_format=cvimodel` 选择 Dashboard Flow，否则使用默认 Flow；部署前更新 `model` 节点字段。
- 应用列表 UI 修复：长名称溢出处理 + Tooltip。
- 一键部署脚本 `scripts/deploy.sh`。
- 训练平台 v2 模型信息接口封装与类型补齐。

### 变更
- `action=train` 改为创建云端应用（`classify_<model>`），部署后在当前页跳转 Dashboard。
- `action=model` 增加 applyModel 响应日志并条件跳转 Dashboard。
- 授权回跳处理优化：`redirect_url` 编码；`sessionStorage` 缓存/清理 action，避免误触发。
- `sensecraftRequest` 对 `code=401` 也触发刷新 token 并重试。

### 修复
- 长应用名不再遮挡编辑/删除按钮。
- 上传过程提供进度、超时提醒与取消入口。
- Dashboard 跳转改为当前页，并基于就绪检测。
