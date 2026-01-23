import axios, { InternalAxiosRequestConfig, AxiosRequestConfig } from "axios";
import {
  getToken,
  setToken,
  getRefreshToken,
  savePlatformInfo,
} from "@/store/platform";
import { refreshTokenApi } from "@/api/sensecraft";
import { sensecraftAuthorize } from "@/utils";

interface SensecraftResponse<T> {
  code: number | string;
  data: T;
  msg?: string;
}

const portalApi = import.meta.env.VITE_PORTAL_URL;
const sensecraftApi = import.meta.env.VITE_SENSECRAFT_URL;
const sensecraftTrainApi = import.meta.env.VITE_SENSECRAFT_TRAIN_API_URL;

// 和平台通信的接口
const sensecraftService = axios.create({});

// 根据api类型修改baseurl
sensecraftService.interceptors.request.use((config) => {
  config.headers.Authorization = getToken();
  if (config.url?.startsWith('portalapi')) {
    config.baseURL = portalApi;
  } else if (config.url?.startsWith('aiserverapi')) {
    config.baseURL = sensecraftApi;
  } else if (config.url?.startsWith('v1/api')) {
    // 模型转换 API 使用独立的 baseURL
    config.baseURL = sensecraftTrainApi;
  } else if (config.url?.startsWith('v2/api')) {
    // 模型转换 API 使用独立的 baseURL
    config.baseURL = sensecraftTrainApi;
  }
  return config;
});

/**
 * @description sensecraftService响应拦截器
 */
sensecraftService.interceptors.response.use(
  async (response) => {
    // blob 响应直接返回，不检查 code
    if (response.config.responseType === "blob") {
      return response;
    }
    const code = response.data?.code;
    if (
      (code == 11101 || code == 11102 || code == 401) &&
      !response.request.responseURL.includes("portalapi/auth/refreshToken")
    ) {
      return handleTokenRefresh(response.config);
    }
    return response;
  },
  async (error) => {
    const { response = {} } = error;
    // blob 错误响应需要特殊处理
    if (
      error.config?.responseType === "blob" &&
      response.data instanceof Blob
    ) {
      try {
        const text = await response.data.text();
        const jsonData = JSON.parse(text);
        const code = jsonData.code;
        if (code === 11101 || code === 11102 || code === 401) {
          return handleTokenRefresh(error.config);
        }
        return Promise.reject(new Error(jsonData.msg || "Download failed"));
      } catch (e) {
        // 解析失败，可能是真正的二进制文件错误，继续抛出原始错误
        return Promise.reject(error);
      }
    }
    const code = response.data?.code;
    if (code === 11101 || code === 11102 || code === 401) {
      return handleTokenRefresh(error.config);
    }
    return Promise.reject(error);
  }
);

async function handleTokenRefresh(config: InternalAxiosRequestConfig) {
  try {
    const newToken = await refreshToken();
    config.headers.Authorization = newToken;
    return sensecraftService.request(config);
  } catch (refreshError) {
    console.error("Token refresh failed:", refreshError);
    return Promise.reject(refreshError);
  }
}

/**
 * 刷新令牌
 * @param config 过期请求配置
 * @returns {any} 返回结果
 */
const refreshToken = async () => {
  const refresh_token = getRefreshToken();
  const token = getToken();

  if (!refresh_token || refresh_token == null || !token || token == null) {
    // 需要登录
    // sensecraftAuthorize();
    return Promise.reject();
  }
  try {
    const resp = await refreshTokenApi({
      refreshToken: refresh_token,
    });
    const newToken = resp.data.token;
    setToken(newToken);
    savePlatformInfo();
    return newToken;
  } catch (error) {
    sensecraftAuthorize();
    return Promise.reject();
  }
};

const createSensecraftRequest = () => {
  return async <T>(
    config: AxiosRequestConfig
  ): Promise<SensecraftResponse<T>> => {
    return await new Promise((resolve, reject) => {
      sensecraftService.request<SensecraftResponse<T> | Blob>(config).then(
        (res) => {
          // blob 响应直接返回，不检查 code
          if (config.responseType === "blob") {
            // 对于 blob 响应，res.data 就是 Blob，需要包装成 SensecraftResponse 格式
            // 但实际上调用方会进行类型断言，所以这里直接返回 res.data
            resolve(res.data as unknown as SensecraftResponse<T>);
            return;
          }
          // 非 blob 响应，检查 code
          const data = res.data as SensecraftResponse<T>;
          if (data.code != 0 && data.code != "0") {
            reject(data);
            return;
          }
          resolve(data);
        },
        (err: unknown) => {
          reject(err);
        }
      );
    });
  };
};

const sensecraftRequest = createSensecraftRequest();

export default sensecraftRequest;
