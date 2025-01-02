import { noderedRequest } from "@/utils/request";

// 获取flow数据
export const getFlows = async () =>
  noderedRequest<[]>({
    url: "flows",
    method: "get",
  });

// 给nodered发送flow数据
export const saveFlows = async (data?: string) =>
  noderedRequest({
    url: "flows",
    method: "post",
    data: data ? data : "[]",
  });

// 获取flow的状态
export const getFlowsState = async () =>
  noderedRequest<{ state: string }>({
    url: "flows/state",
    method: "get",
  });

// 设置flow的状态
export const setFlowsState = async (data: { state: string }) =>
  noderedRequest({
    url: "flows/state",
    method: "post",
    data: JSON.stringify(data),
  });
