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
  // 使用 supervisorRequest 以便自动携带鉴权，且支持设备 baseURL
  const blobResponse = await supervisorRequest<Blob>(
    {
      url: `/api/fileMgr/download`,
      method: "post",
      data: { storage, path, t: Date.now() },
      responseType: "blob",
    },
    { catchs: true }
  );
  // catchs: true 时返回的结构是后端原始响应体；此处约定为 Blob
  return blobResponse as unknown as Blob;
};

// 统一的分片上传接口
export const uploadFiles = async (
  storage: StorageType,
  path: string,
  files: FileList,
  onProgress?: (totalProgress: number) => void
): Promise<void> => {
  const CHUNK_SIZE = 1024 * 1024; // 1MB per chunk
  const totalFiles = files.length;
  let completedFiles = 0;

  for (let i = 0; i < totalFiles; i++) {
    const file = files[i];
    const totalSize = file.size;
    let offset = 0;

    console.log(`开始分片上传文件: ${file.name}, 大小: ${file.size} bytes`);

    // 对于每个文件进行分片上传
    while (offset < totalSize) {
      const end = Math.min(offset + CHUNK_SIZE, totalSize);
      const chunk = file.slice(offset, end);

      const formData = new FormData();
      formData.append("file", chunk, file.name);
      formData.append("storage", storage);
      formData.append("offset", offset.toString());
      if (path) {
        formData.append("path", path);
      }

      try {
        await supervisorRequest<object>({
          url: `/api/fileMgr/upload`,
          method: "post",
          data: formData,
        });

        console.log(`上传分片成功: ${offset} - ${end} for ${file.name}`);
        offset = end;

        // 计算并报告总进度
        if (onProgress) {
          const currentFileProgress = offset / totalSize;
          const totalProgress =
            ((completedFiles + currentFileProgress) / totalFiles) * 100;
          onProgress(totalProgress);
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
    console.log(`文件上传完成: ${file.name}`);
  }

  console.log(`所有文件上传完成，共 ${totalFiles} 个文件`);
};

// 兼容旧类型命名
export type DirectoryEntry = FileEntry;
