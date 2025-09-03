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
