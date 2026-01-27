# SenseCraft 连接与 Node-RED/模型载入实现说明

本文件描述 SenseCraft 平台通过 reCamera 前端与设备交互，完成 Node-RED 工作流与模型同步的实现路径与关键代码位置。

## 1. 入口与授权

- 入口页面：`/workspace`（`src/views/workspace/index.tsx`）。
- 前端从 URL 参数读取 `token`、`refresh_token`、`action`、`app_id`、`model_id` 等，用于触发不同同步动作。
- 授权跳转由 `sensecraftAuthorize()` 生成（`src/utils/index.ts`）。
- 平台 token 由 `src/store/platform.ts` 管理，并通过 `savePlatformInfo()` 写回设备（`api/deviceMgr/savePlatformInfo`）。

## 2. 核心动作类型（action）

`action` 由 SenseCraft 平台拼接到设备前端 URL，用于驱动行为：

- `app`：加载指定云端应用。
- `new`：创建新应用并覆盖设备当前应用。
- `clone`：克隆公开应用到设备并创建新的应用副本。
- `model`：部署指定模型并生成默认 flow。
- `train`：下载训练模型并上传到设备，更新 flow。
- `normal`：默认模式，对比本地与云端应用并提示同步。

处理逻辑在 `src/views/workspace/index.tsx` 的 `initAction()` 内。

## 3. 云端 -> 设备同步（载入 Flow + Model）

### 3.1 云端应用载入

- `viewAppApi(app_id)` 获取应用详情（`src/api/sensecraft/index.ts`）。
- `syncCloudAppToLocal()` 将云端数据同步到设备（`src/views/workspace/index.tsx`）。

### 3.2 模型下发到设备

- `uploadModel(model_data)`：
  - 如果模型是预置模型或具备下载 URL，则拉取文件并上传。
  - 上传接口：`uploadModelApi`（`src/api/device/index.ts`），支持分片上传。

### 3.3 Node-RED Flow 下发

- `sendFlow()` -> `saveFlows()`（`src/api/nodered/index.ts`）。
- 通过 Node-RED `/flows` API 写入完整 flow，并确保 `flows/state` 为 `start`。

## 4. 设备 -> 云端同步（保存 Flow + Model）

### 4.1 拉取设备流与模型信息

- `getFlows()` 读取 Node-RED flows。
- `getModelInfoApi()` 读取设备当前模型描述信息与 MD5。

### 4.2 本地模型上传到云端

当本地模型 `model_id == "0"` 且云端无 URL 或 MD5 变化时：

- 从设备下载模型文件：
  - `GET http://<device-ip>/api/deviceMgr/getModelFile`
- 向 SenseCraft 获取上传地址：
  - `acquireFileUrlApi(file_name)`（`src/api/sensecraft/index.ts`）
- PUT 上传模型文件到云端。
- 更新应用：`updateAppApi()`，保存 flow + model 信息。

同步入口：`syncLocalAppToCloud()`（`src/views/workspace/index.tsx`）。

## 5. 模型部署与训练

### 5.1 平台模型部署

- `applyModelApi(model_id)` 返回 `model_snapshot`。
- `uploadModel(model_snapshot)` 上传至设备。
- `createAppAndUpdateFlow()` 根据 `model_snapshot.arguments.task` 选择默认 flow：
  - `classify` → `DefaultFlowDataWithDashboard`
  - 其它 → `DefaultFlowData`

### 5.2 平台训练模型下载

- `GET /v2/api/get_model` 获取训练模型信息（device_type/classes/task_id 等）。
- `getModelApi(model_id, { task_id, device_type })` 下载模型文件。
- 上传到设备：`uploadModelApi()`（携带 `model_info`，包含 `model_id/model_name/classes`）。
- 使用 `DefaultFlowDataWithDashboard` 作为新应用的 flow，并将 `model` 节点 `classes`/`uri` 更新后部署。
- 创建云端应用并同步（`createAppAndUpdateFlow`），应用名自动生成：`classify_<model_name>`。
- 部署完成后轮询 `http://<ip>:1880/dashboard`，就绪后在当前页跳转到 `/#/dashboard`。

入口：`handleTrainModel()`（`src/views/workspace/index.tsx`）。

## 6. 关键代码文件

- 入口与同步逻辑：`src/views/workspace/index.tsx`
- SenseCraft API：`src/api/sensecraft/index.ts`
- Node-RED API：`src/api/nodered/index.ts`
- 设备 API（模型上传等）：`src/api/device/index.ts`
- 平台授权与环境变量：`src/utils/index.ts`、`.env.*`
- 平台 token 管理：`src/store/platform.ts`

## 7. 运行关系图（简化）

1) SenseCraft -> 设备前端 URL（携带 action + token）
2) 前端 -> SenseCraft API（拉取应用/模型）
3) 前端 -> 设备 API（上传模型）
4) 前端 -> Node-RED API（写入 flow）
5) 可选：设备 -> SenseCraft（本地 flow/model 反向同步）

## 8. 本次改动记录（Train 流程与默认 Flow）

- `train` 模式默认 flow 改为 `DefaultFlowDataWithDashboard`，当设备无 flow 或无 `model` 节点时直接导入该默认 flow。
- `train` 模型信息获取统一使用 `GET /v2/api/get_model`。
- `train` 上传 `model_info` 时携带 `classes` 数组，并在默认 flow 的 `model` 节点写入 `classes`（逗号分隔）避免编辑器报错。
- 训练模型 `device_type != 40` 时弹窗提示并停止流程，不跳转 dashboard。
- `train` 创建云端应用并同步，应用名自动生成 `classify_<model_name>`。
- `train` 部署完成后轮询 `http://<ip>:1880/dashboard`，就绪后在当前页跳转到 `/#/dashboard`。

## 9. 一键部署脚本与使用说明

脚本路径：`scripts/deploy.sh`

功能：
- 本地构建 `dist`。
- 上传到设备临时目录（默认 `/home/recamera/dist`）。
- 覆盖设备 Web 目录（默认 `/usr/share/supervisor/www`）。
- 重启设备。

基础用法：

```bash
chmod +x scripts/deploy.sh
./scripts/deploy.sh 192.168.118.90 root # 建议用 root 账号
```

可选环境变量：
- `SSH_PORT`：SSH 端口（默认 22）
- `REMOTE_TMP_DIR`：设备临时目录（默认 `/home/<user>/dist`）
- `REMOTE_WWW_DIR`：设备 Web 目录（默认 `/usr/share/supervisor/www`）
- `BUILD_CMD`：构建命令（默认 `npm run build`）
- `DIST_DIR`：构建产物目录（默认 `dist`）

注意事项：
- 如果使用普通用户（如 `recamera`），设备上需要可执行 `sudo`，否则替换目录会失败。
- 若使用 `root` 账号，可将 `sudo` 去掉以避免交互密码。***

## 10. 其它改动记录

- `DefaultFlowDataWithDashboard` 拆分到独立模块 `src/utils/flowDefaults.ts`，由 `src/utils/index.ts` 重新导出。
- 授权 `redirect_url` 使用编码，回跳时通过 `sessionStorage` 恢复 `action/model_id` 等参数。
- `sensecraft_action` 在 `action=train` 触发后清理，避免手动进入 workspace 误触发 train。
- App 列表增加溢出处理与 Tooltip，长名称不遮挡编辑/删除按钮。
