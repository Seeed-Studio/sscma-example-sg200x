export enum PowerMode {
  Shutdown = 0, // Shutdown
  Restart = 1, // Restart
}
export enum DeviceChannleMode {
  Official = 0, // Official
  Self = 1, // Custom
}
export enum DeviceNeedRestart {
  No = 0, // No restart needed
  Yes = 1, // Restart needed
}

export enum SystemUpdateStatus {
  Normal = 1, // Normal, no update needed
  Updating = 2, // Updating
  Checking = 3, // Checking
}

export enum ServiceStatus {
  RUNNING = 0, // Running normally
  STARTING = 1, // Starting
  FAILED = 2, // Failed to start
  ERROR = 4, // Runtime error
}

export enum UpdateStatus {
  Check = "Check",
  NeedUpdate = "NeedUpdate", // New version available
  NoNeedUpdate = "NoNeedUpdate", // No update needed
  Updating = "Updating", // Updating
  UpdateDone = "UpdateDone", // Update complete
}
