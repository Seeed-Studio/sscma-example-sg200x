import { supervisorRequest } from "@/utils/request";
import { WifiEnable } from "@/enum/network";
import { IWifiInfo, IConnectParams } from "./network";

// 获取wifi信息列表（包含所有WiFi相关信息）
export const getWiFiInfoListApi = async () =>
  supervisorRequest<{
    wifiEnable: WifiEnable; // 0、有wifi，未开启 1、有wifi，已开启 2、没有wifi
    etherInfo?: IWifiInfo; // 连接的有线网信息
    connectedWifiInfoList?: IWifiInfo[]; // 已经连接过的wifi列表
    wifiInfoList?: IWifiInfo[]; // 扫描到的未连接过的wifi列表
  }>({
    url: "api/wifiMgr/getWiFiInfoList",
    method: "get",
  });
// 连接wifi
export const connectWifiApi = async (data: IConnectParams) =>
  supervisorRequest({
    url: "api/wifiMgr/connectWiFi",
    method: "post",
    data,
  });
// 断开wifi
export const disconnectWifiApi = async (data: { ssid: string }) =>
  supervisorRequest({
    url: "api/wifiMgr/disconnectWiFi",
    method: "post",
    data,
  });
// 启停wifi
export const switchWiFiApi = async (data: { mode: WifiEnable }) =>
  supervisorRequest({
    url: "api/wifiMgr/switchWiFi",
    method: "post",
    data,
  });
// wifi忘记
export const forgetWiFiApi = async (data: { ssid: string }) =>
  supervisorRequest({
    url: "api/wifiMgr/forgetWiFi",
    method: "post",
    data,
  });
