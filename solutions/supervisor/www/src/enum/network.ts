// WiFi encryption status
export enum WifiAuth {
  NoNeed = 0,
  Need = 1,
}

// IP assignment rule
export enum WifiIpAssignmentRule {
  Automatic = 1, // Dynamic
  Static = 0, // Static
}

// WiFi connection status (previously connected)
export enum WifiConnectedStatus {
  No = 0,
  Yes = 1,
}

// WiFi enable status (corresponds to getWiFiInfoList wifiEnable field)
export enum WifiEnable {
  Close = 0, // Has WiFi, not enabled
  Open = 1, // Has WiFi, enabled
  Disable = 2, // No WiFi
}

// Network connection status (corresponds to WiFiInfo status field)
export enum NetworkStatus {
  Disconnected = 1, // Disconnected
  Connecting = 2, // Connecting
  Connected = 3, // Connected
}
