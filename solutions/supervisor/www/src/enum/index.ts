export enum PowerMode {
  Shutdown = 0, //关机
  Restart = 1, //重启
}
export enum DeviceChannleMode {
  Official = 0, //官方
  Self = 1, //自定义
}
export enum DeviceNeedRestart {
  No = 0, //不需要
  Yes = 1, //需要
}

export enum SystemUpdateStatus {
  Normal = 1, //正常,不需要升级
  Updating = 2, //升级中
  Checking = 3, //检查中
}

export enum ServiceStatus {
  RUNNING = 0, //运行正常
  STARTING = 1, //启动中
  FAILED = 2, //启动失败
  ERROR = 4, //运行异常
}

export enum UpdateStatus {
  Check = "Check",
  NeedUpdate = "NeedUpdate", //有新版本需要更新
  NoNeedUpdate = "NoNeedUpdate", //不需要更新
  Updating = "Updating", //更新中
  UpdateDone = "UpdateDone", //更新完毕
}

// 模型转换状态
export enum ModelConversionStatus {
  Init = "init", // 初始化
  Done = "done", // 转换完成
  Error = "error", // 转换失败
  // 数值字符串表示进行中（进度百分比）
}
