import { sensecraftRequest } from "@/utils/request";
import {
  ICreateTaskParams,
  ICreateTaskResponse,
  IGetTrainingRecordsParams,
  IGetTrainingRecordsResponse,
  IGetTrainStatusResponse,
  IGetModelInfoResponse,
  IGetModelV2Response,
} from "./model.d";

/**
 * 创建模型转换任务（ONNX → CVIMODEL）
 * POST /v1/api/create_task
 */
export const createTaskApi = async (params: ICreateTaskParams) => {
  const formData = new FormData();
  formData.append("framework_type", String(params.framework_type));
  formData.append("device_type", String(params.device_type));
  formData.append("file", params.file);

  if (params.precision) {
    formData.append("precision", params.precision);
  }
  if (params.dataset_file) {
    formData.append("dataset_file", params.dataset_file);
  }
  if (params.prompt) {
    formData.append("prompt", params.prompt);
  }

  return sensecraftRequest<ICreateTaskResponse>({
    url: "v1/api/create_task",
    method: "post",
    data: formData,
    // axios 会自动为 FormData 设置正确的 Content-Type（包括 boundary）
  });
};

/**
 * 获取用户的模型列表
 * GET /v1/api/get_training_records
 */
export const getTrainingRecordsApi = async (
  params: IGetTrainingRecordsParams
) =>
  sensecraftRequest<IGetTrainingRecordsResponse>({
    url: "v1/api/get_training_records",
    method: "get",
    params,
  });

/**
 * 获取转换任务当前状态
 * GET /v1/api/train_status
 */
export const getTrainStatusApi = async (model_id: string) =>
  sensecraftRequest<IGetTrainStatusResponse>({
    url: "v1/api/train_status",
    method: "get",
    params: { model_id },
  });

/**
 * 获取转换完成的模型
 * GET /v1/api/get_model
 * 返回二进制文件流
 */
export const getModelApi = async (
  model_id: string,
  params?: { device_type?: number; task_id?: number }
): Promise<Blob> => {
  // 使用 sensecraftRequest 以便自动携带鉴权，且支持 baseURL
  const blobResponse = await sensecraftRequest<Blob>({
    url: "v1/api/get_model",
    method: "get",
    params: {
      model_id: model_id,
      ...params,
    },
    responseType: "blob",
  });
  // responseType: "blob" 时返回的结构是后端原始响应体；此处约定为 Blob
  return blobResponse as unknown as Blob;
};

/**
 * 获取单个模型信息
 * GET /v1/api/get_model_info
 */
export const getModelInfoApi = async (
  model_id: string,
  params?: { task_id?: number; device_type?: number }
) =>
  sensecraftRequest<IGetModelInfoResponse>({
    url: "v1/api/get_model_info",
    method: "get",
    params: {
      model_id,
      ...params,
    },
  });

/**
 * 获取模型信息（v2）
 * GET /v2/api/get_model
 */
export const getModelV2Api = async (
  model_id: string,
  params?: { task_id?: number; device_type?: number }
) =>
  sensecraftRequest<IGetModelV2Response>({
    url: "v2/api/get_model",
    method: "get",
    params: {
      model_id,
      ...params,
    },
  });

/**
 * 删除模型
 * GET /v1/api/del_model
 */
export const deleteModelApi = async (model_id: string) =>
  sensecraftRequest<{ model_id: string }>({
    url: "v1/api/del_model",
    method: "get",
    params: { model_id },
  });

/**
 * 停止转换任务
 * GET /v1/api/stop_task
 */
export const stopTaskApi = async (model_id: string) =>
  sensecraftRequest<{ model_id: string }>({
    url: "v1/api/stop_task",
    method: "get",
    params: { model_id },
  });
