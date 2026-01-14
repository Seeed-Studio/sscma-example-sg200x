import { supervisorRequest } from "@/utils/request";
import { WifiEnable, HalowEnable, AntennaEnable } from "@/enum/network";
import { IWifiInfo, IConnectParams, IConnectHalowParams } from "./network";

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

// 获取halow信息列表（包含所有Halow相关信息）
export const getHalowInfoListApi = async () =>
  supervisorRequest<{
    halowEnable: HalowEnable; // 0、有halow，未开启 1、有halow，已开启 2、没有halow
    antennaEnable?: AntennaEnable; // 天线状态 0 - RF1, 1 - RF2
    currentHalowInfo?: IWifiInfo; // 当前halow信息，包含连接状态
    connectedHalowInfoList?: IWifiInfo[]; // 已经连接过的halow列表
    halowInfoList?: IWifiInfo[]; // 扫描到的未连接过的halow列表
  }>({
    url: "api/halowMgr/getHalowInfoList",
    method: "get",
  });

// 连接halow
export const connectHalowApi = async (data: IConnectHalowParams) =>
  supervisorRequest({
    url: "api/halowMgr/connectHalow",
    method: "post",
    data,
  });

// 断开halow
export const disconnectHalowApi = async (data: { ssid: string }) =>
  supervisorRequest({
    url: "api/halowMgr/disconnectHalow",
    method: "post",
    data,
  });

// 启停halow
export const switchHalowApi = async (data: {
  mode: HalowEnable; // 0-关闭, 1-开启
}) =>
  supervisorRequest({
    url: "api/halowMgr/switchHalow",
    method: "post",
    data,
  });

// 切换天线
export const switchAntennaApi = async (data: {
  mode: AntennaEnable; // 0 - RF1, 1 - RF2
}) =>
  supervisorRequest({
    url: "api/halowMgr/switchAntenna",
    method: "post",
    data,
  });

// halow忘记
export const forgetHalowApi = async (data: { ssid: string }) =>
  supervisorRequest({
    url: "api/halowMgr/forgetHalow",
    method: "post",
    data,
  });
