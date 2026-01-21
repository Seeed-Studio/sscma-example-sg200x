import {
  WifiConnectedStatus,
  WifiIpAssignmentRule,
  NetworkStatus,
} from "@/enum/network";

interface IWifiInfo {
  ssid: string;
  auth: number;
  signal: number;
  connectedStatus: WifiConnectedStatus;
  status: NetworkStatus;
  macAddress: string;
  ip: string;
  ipAssignment: WifiIpAssignmentRule;
  subnetMask: string;
  dns1?: string;
  dns2?: string;
  loading?: boolean;
}

interface IConnectParams {
  ssid: string;
  password?: string;
}

// Halow 配置信息（用于连接 Halow 时的配置）
interface IHalowInfo {
  country: "US" | "AU"; // 国家
  mode: 0 | 1; // 0：no WDS, 1: WDS
  encryption: "WPA3-SAE" | "OWE" | "No encryption"; // 加密方法
}

interface IConnectHalowParams {
  ssid: string; // wifi halow name
  password?: string; // wifi halow 密码。可选，如果没有加密的wifi或者已经连接过的wifi这不需要传
  halowInfo?: IHalowInfo; // Wi-Fi halow 配置信息（可选，重连已保存的网络时可不传）
}
