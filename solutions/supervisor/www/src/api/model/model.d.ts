// 模型转换任务相关类型定义
import { ModelConversionStatus } from "@/enum";

// 创建任务请求参数
export interface ICreateTaskParams {
  framework_type: number; // 固定值：8（ONNX→CVIMODEL）
  device_type: number; // 固定值：40（reCamera）
  file: File; // 待转换的模型文件，后缀必须为 .onnx
  precision?: string; // 默认 int8. 支持f16(无法运行)，mixed
  dataset_file?: File; // 用于量化的图片数据集（zip 格式）
  prompt?: string; // 对转换无影响，可以用于保存原始文件的名称
}

// 创建任务响应
export interface ICreateTaskResponse {
  model_id: string; // 任务 / 模型的唯一标识
  created_at: string;
  model_size: string | null;
}

// 模型记录
export interface IModelRecord {
  user_id: string;
  model_id: string;
  prompt: string;
  progress: number; // 0~100
  framework_type: number;
  device_type: number;
  error_message: string | null;
  status: ModelConversionStatus; // 状态：init、done、error
  started_at: string; // Unix 毫秒时间戳
  finished_at: string; // Unix 毫秒时间戳
  created_at: string; // Unix 毫秒时间戳
  model_size: string; // 模型大小（字节，字符串）
}

// 获取模型列表响应
export interface IGetTrainingRecordsResponse {
  total: number;
  records: IModelRecord[];
  undone: IModelRecord[];
}

// 获取模型列表请求参数
export interface IGetTrainingRecordsParams {
  framework_type: number; // 固定值：8
  device_type: number; // 固定值：40
  page: number;
  size: number;
}

// 获取任务状态响应
export interface IGetTrainStatusResponse {
  status: ModelConversionStatus | number; // 状态可能是数字字符串（进行中）
  error_message: string | null;
  modelId: string;
  prompt: string;
  framework_type: number;
  started_at: string; // Unix 毫秒时间戳
  finished_at: string; // Unix 毫秒时间戳
}

export interface IModelMetadata {
  classes?: string[];
}

export interface IGetModelInfoResponse {
  model_id: string;
  task_id?: number;
  display_name?: string;
  display_ext?: string;
  prompt?: string;
  framework_type?: number;
  device_type?: number;
  status?: ModelConversionStatus;
  progress?: number;
  error_message?: string | null;
  started_at?: string;
  finished_at?: string;
  created_at?: string;
  model_size?: string;
  metadata?: IModelMetadata;
}

export interface IGetModelV2Response {
  model_id: string;
  task_id?: number;
  display_name?: string;
  display_ext?: string;
  framework_type?: number;
  device_type?: number;
  status?: ModelConversionStatus;
  download_url?: string;
  download_filename?: string;
  md5?: string;
  model_size?: string;
  classes?: string[];
  metadata?: IModelMetadata;
}
