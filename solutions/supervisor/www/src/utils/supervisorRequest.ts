import axios, { AxiosRequestConfig, AxiosError } from "axios";
import { Toast } from "antd-mobile";
import { getToken, refreshToken } from "@/store/user";
import { isDev } from "@/utils";

// 根据环境设置 baseURL
const baseIP = isDev ? "http://192.168.42.1" : window.location.origin;

// 和设备通信的
const supervisorService = axios.create({
  baseURL: baseIP,
});

// 设置token
supervisorService.interceptors.request.use((config) => {
  config.headers.Authorization = getToken();
  return config;
});

interface BaseResponse<T> {
  code: number | string;
  data: T;
  msg?: string;
  errorcode?: number;
  message?: string;
  timestamp?: string;
}
interface OtherRequestConfig {
  loading?: boolean;
  catchs?: boolean;
}

const createSupervisorRequest = () => {
  return async <T>(
    config: AxiosRequestConfig,
    otherConfig: OtherRequestConfig = {
      loading: false,
      catchs: false,
    }
  ): Promise<BaseResponse<T>> => {
    const { loading, catchs } = otherConfig;
    let toast: any;
    if (loading) {
      toast = true;
      Toast.show({ duration: 0, icon: "loading" });
    }

    return await new Promise((resolve, reject) => {
      supervisorService.request<BaseResponse<T>>(config).then(
        (res) => {
          if (catchs) {
            resolve(res.data);
          } else {
            toast && Toast.clear();
            if (res.data.code !== 0 && res.data.code !== "0") {
              Toast.show({
                icon: "fail",
                content: res.data.msg || "请求失败",
              });
              reject(res.data);
              return;
            }
            resolve(res.data);
          }
        },
        (err: AxiosError) => {
          console.error(err);
          //鉴权失败，重新登录获取token
          if (err.response?.status == 401) {
            refreshToken();
          } else {
            if (!catchs) {
              toast && Toast.clear();
              Toast.show({
                icon: "fail",
                content: err.message || "请求失败",
              });
            }
          }
          reject(err);
        }
      );
    });
  };
};

const supervisorRequest = createSupervisorRequest();

export default supervisorRequest;
