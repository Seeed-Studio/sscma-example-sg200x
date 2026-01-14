// wifi是否加密
export enum WifiAuth {
  NoNeed = 0,
  Need = 1,
}

// IP 分配规则
export enum WifiIpAssignmentRule {
  Automatic = 1, //动态
  Static = 0, //静态
}

// wifi是否连接过
export enum WifiConnectedStatus {
  No = 0,
  Yes = 1,
}

// wifi是否开启（对应getWiFiInfoList接口的wifiEnable字段）
export enum WifiEnable {
  Close = 0, //有wifi，未开启
  Open = 1, //有wifi，已开启
  Disable = 2, //没有wifi
}

// 网络连接状态（对应WiFiInfo里面的status字段）
export enum NetworkStatus {
  Disconnected = 1, //未连接
  Connecting = 2, //连接中
  Connected = 3, //已连接
}

// halow是否开启（对应getHalowInfoList接口的halowEnable字段）
export enum HalowEnable {
  Close = 0, //有halow，未开启
  Open = 1, //有halow，已开启
  Disable = 2, //没有halow
}

// 天线状态（对应getHalowInfoList接口的antennaEnable字段）
export enum AntennaEnable {
  RF1 = 0, // RF1
  RF2 = 1, // RF2
}
