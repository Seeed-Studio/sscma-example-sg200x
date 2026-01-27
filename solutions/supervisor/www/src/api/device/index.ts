import { supervisorRequest } from "@/utils/request";
import { PowerMode, DeviceChannleMode, SystemUpdateStatus } from "@/enum";
import {
  IDeviceInfo,
  IChannelParams,
  IServiceStatus,
  IIPDevice,
} from "./device";

// 获取设备信息
export const queryDeviceInfoApi = async () =>
  supervisorRequest<IDeviceInfo>(
    {
      url: "api/deviceMgr/queryDeviceInfo",
      method: "get",
    },
    {
      catchs: true,
    }
  );

export const getDeviceListApi = async () =>
  supervisorRequest<{
    deviceList: IIPDevice[];
  }>(
    {
      url: "api/deviceMgr/getDeviceList",
      method: "get",
    },
    {
      catchs: true,
    }
  );

// 获取设备运行状态
export const queryServiceStatusApi = async () =>
  supervisorRequest<IServiceStatus>(
    {
      url: "api/deviceMgr/queryServiceStatus",
      method: "get",
      timeout: 5000,
    },
    {
      catchs: true,
    }
  );

// 修改设备信息
export const updateDeviceInfoApi = async (data: { deviceName: string }) =>
  supervisorRequest<IDeviceInfo>({
    url: "api/deviceMgr/updateDeviceName",
    method: "post",
    data,
  });

// 修改渠道信息
export const changeChannleApi = async (data: IChannelParams) =>
  supervisorRequest({
    url: "api/deviceMgr/updateChannel",
    method: "post",
    data,
  });
// 设备重启与关机
export const setDevicePowerApi = async (data: { mode: PowerMode }) =>
  supervisorRequest({
    url: "api/deviceMgr/setPower",
    method: "post",
    data,
  });
// 更新设备系统
export const updateSystemApi = async () =>
  supervisorRequest({
    url: "api/deviceMgr/updateSystem",
    method: "post",
  });
// 获取设备更新进度
export const getUpdateSystemProgressApi = async () =>
  supervisorRequest<{
    progress: number;
  }>({
    url: "api/deviceMgr/getUpdateProgress",
    method: "get",
  });
// 更新设备系统
export const cancelUpdateApi = async () =>
  supervisorRequest({
    url: "api/deviceMgr/cancelUpdate",
    method: "post",
  });

// 获取设备更新版本信息
export const getSystemUpdateVesionInfoApi = async (data: {
  url: string;
  channel?: DeviceChannleMode;
}) =>
  supervisorRequest<{
    osName: string;
    osVersion: string;
    status: SystemUpdateStatus;
  }>(
    {
      url: "api/deviceMgr/getSystemUpdateVersion",
      method: "post",
      data,
    },
    {
      catchs: true,
    }
  );

// 获取模型信息
export const getModelInfoApi = async () =>
  supervisorRequest<IDeviceInfo>(
    {
      url: "api/deviceMgr/getModelInfo",
      method: "get",
    },
    {
      catchs: true,
    }
  );

// 上传模型（支持分片上传）
export const uploadModelApi = async (
  data: FormData,
  onProgress?: (progress: number) => void,
  signal?: AbortSignal
) => {
  const CHUNK_SIZE = 512 * 1024; // 512KB
  
  // 提取文件和其他数据
  let modelFile: File | Blob | null = null;
  let modelInfo: string | null = null;
  
  for (const [key, value] of data.entries()) {
    if (key === "model_file" && (value instanceof File || value instanceof Blob)) {
      modelFile = value;
    } else if (key === "model_info" && typeof value === "string") {
      modelInfo = value;
    }
  }
  
  // 如果没有模型文件或文件小于阈值，使用传统上传
  if (!modelFile || modelFile.size <= CHUNK_SIZE) {
    return supervisorRequest<IDeviceInfo>({
      url: "api/deviceMgr/uploadModel",
      method: "post",
      data,
      signal,
    });
  }
  
  // 分片上传大文件
  const totalSize = modelFile.size;
  let offset = 0;
  
  while (offset < totalSize) {
    const chunk = modelFile.slice(offset, offset + CHUNK_SIZE);
    const chunkFormData = new FormData();
    
    chunkFormData.append("model_file", chunk);
    chunkFormData.append("offset", offset.toString());
    chunkFormData.append("size", totalSize.toString());
    
    // 只在最后一个分片附加 model_info
    if (offset + CHUNK_SIZE >= totalSize && modelInfo) {
      chunkFormData.append("model_info", modelInfo);
    }
    
    const response = await supervisorRequest<IDeviceInfo>({
      url: "api/deviceMgr/uploadModel",
      method: "post",
      data: chunkFormData,
      signal,
    });
    
    offset += CHUNK_SIZE;
    
    // 报告进度
    if (onProgress) {
      const progress = Math.min((offset / totalSize) * 100, 100);
      onProgress(progress);
    }
    
    // 如果这是最后一个分片，返回响应
    if (offset >= totalSize) {
      return response;
    }
  }
  
  // 理论上不会到达这里，但为了类型安全
  throw new Error("Upload failed");
};

// 保存平台信息
export const savePlatformInfoApi = async (data: { platform_info: string }) =>
  supervisorRequest({
    url: "api/deviceMgr/savePlatformInfo",
    method: "post",
    data,
  });

// 获取平台信息
export const getPlatformInfoApi = async () =>
  supervisorRequest<{
    platform_info: string;
  }>(
    {
      url: "api/deviceMgr/getPlatformInfo",
      method: "get",
    },
    {
      catchs: true,
    }
  );
