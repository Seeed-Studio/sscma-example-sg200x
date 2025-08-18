// 文件管理相关API接口 - 接入设备后端 /api/fileMgr
import supervisorRequest from "@/utils/supervisorRequest";

export type StorageType = "local" | "sd";
export type FileType = "directory" | "file";

export interface FileEntry {
  name: string;
  size?: number;
  modified?: number; // seconds timestamp
  storage?: StorageType;
  type: FileType;
}

export interface FileListData {
  path: string;
  storage: StorageType;
  directories: FileEntry[];
  files: FileEntry[];
}

export interface BaseResponse<T> {
  code: number | string;
  data: T;
  msg?: string;
}

export const listFiles = async (
  storage: StorageType,
  path: string = ""
): Promise<FileListData> => {
  const res = await supervisorRequest<FileListData>({
    url: `/api/fileMgr/list`,
    method: "post",
    data: { storage, path },
  });
  return res.data;
};

export const checkSdAvailable = async (): Promise<boolean> => {
  // 使用 catchs 获取原始响应，便于识别 code = -1
  const res = await supervisorRequest<FileListData>(
    {
      url: `/api/fileMgr/list`,
      method: "post",
      data: { storage: "sd", path: "" },
    },
    { catchs: true }
  );
  return res.code === 0 || res.code === "0";
};

export const makeDirectory = async (storage: StorageType, path: string) => {
  const res = await supervisorRequest<object>({
    url: `/api/fileMgr/mkdir`,
    method: "post",
    data: { storage, path },
  });
  return res.data;
};

export const removeEntry = async (storage: StorageType, path: string) => {
  const res = await supervisorRequest<object>({
    url: `/api/fileMgr/remove`,
    method: "post",
    data: { storage, path },
  });
  return res.data;
};

export const renameEntry = async (
  storage: StorageType,
  oldPath: string,
  newPath: string
) => {
  const res = await supervisorRequest<object>({
    url: `/api/fileMgr/rename`,
    method: "post",
    data: { storage, old_path: oldPath, new_path: newPath },
  });
  return res.data;
};

export const getInfo = async (storage: StorageType, path: string) => {
  const res = await supervisorRequest<{
    path: string;
    storage: StorageType;
    is_directory: boolean;
    size?: number;
    modified?: number;
  }>({
    url: `/api/fileMgr/info`,
    method: "post",
    data: { storage, path },
  });
  return res.data;
};

export const downloadAsBlob = async (
  storage: StorageType,
  path: string
): Promise<Blob> => {
  const url = `/api/fileMgr/download?path=${encodeURIComponent(
    path
  )}&storage=${encodeURIComponent(storage)}`;

  // 使用 supervisorRequest 以便自动携带鉴权，且支持设备 baseURL
  const blobResponse = await supervisorRequest<Blob>(
    {
      url,
      method: "post",
      responseType: "blob",
    },
    { catchs: true }
  );
  // catchs: true 时返回的结构是后端原始响应体；此处约定为 Blob
  return blobResponse as unknown as Blob;
};

// 上传进度信息接口
export interface UploadProgressInfo {
  currentFileIndex: number;
  currentFileName: string;
  currentFileProgress: number; // 0-100
  totalFiles: number;
  completedFiles: number;
}

// 统一的分片上传接口
export const uploadFiles = async (
  storage: StorageType,
  path: string,
  files: FileList,
  onProgress?: (progressInfo: UploadProgressInfo) => void
): Promise<void> => {
  const CHUNK_SIZE = 1024 * 1024; // 1MB per chunk
  const totalFiles = files.length;
  let completedFiles = 0;

  for (let i = 0; i < totalFiles; i++) {
    const file = files[i];
    const totalSize = file.size;
    let offset = 0;

    // 对于每个文件进行分片上传
    while (offset < totalSize) {
      const end = Math.min(offset + CHUNK_SIZE, totalSize);
      const chunk = file.slice(offset, end);

      // 手动构建URL参数，确保编码一致性
      const uploadUrl = path
        ? `/api/fileMgr/upload?storage=${encodeURIComponent(
            storage
          )}&offset=${encodeURIComponent(
            offset.toString()
          )}&path=${encodeURIComponent(path)}`
        : `/api/fileMgr/upload?storage=${encodeURIComponent(
            storage
          )}&offset=${encodeURIComponent(offset.toString())}`;

      // 只保留文件数据在FormData中
      const formData = new FormData();
      formData.append("file", chunk, file.name);

      try {
        await supervisorRequest<object>({
          url: uploadUrl,
          method: "post",
          data: formData,
        });

        offset = end;

        // 计算并报告详细进度信息
        if (onProgress) {
          const currentFileProgress = (offset / totalSize) * 100;
          onProgress({
            currentFileIndex: i,
            currentFileName: file.name,
            currentFileProgress: Math.round(currentFileProgress),
            totalFiles,
            completedFiles,
          });
        }
      } catch (error) {
        console.error(
          `分片上传失败 (${offset}-${end}) for ${file.name}:`,
          error
        );
        throw new Error(`分片上传失败: ${error}`);
      }
    }

    completedFiles++;
  }
};

// 兼容旧类型命名
export type DirectoryEntry = FileEntry;
