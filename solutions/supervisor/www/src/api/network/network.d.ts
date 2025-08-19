import {
  WifiConnectedStatus,
  WifiAutoConnect,
  WifiIpAssignmentRule,
  NetworkStatus,
} from "@/enum/network";

interface IWifiInfo {
  ssid: string;
  auth: number;
  signal: number;
  connectedStatus: WifiConnectedStatus;
  status: NetworkStatus;
  connectChecked: boolean;
  connectedStatusText: string;
  macAddress: string;
  ip: string;
  ipAssignment: WifiIpAssignmentRule;
  type: number;
  autoConnect: WifiAutoConnect;
  subnetMask: string;
  ipAssignmentText: string;
  loading?: boolean;
}

interface IConnectParams {
  ssid: string;
  password?: string;
}
