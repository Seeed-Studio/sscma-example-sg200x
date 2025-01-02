import { useEffect, Reducer, useReducer, useRef } from "react";
import { FormInstance } from "antd-mobile/es/components/form";
import useConfigStore from "@/store/config";
import {
  DeviceChannleMode,
  PowerMode,
  DeviceNeedRestart,
  IsUpgrading,
} from "@/enum";

import { IChannelParams } from "@/api/device/device";
import {
  getDeviceInfoApi,
  changeChannleApi,
  setDevicePowerApi,
  updateSystemApi,
  getUpdateSystemProgressApi,
  cancelUpdateApi,
  getSystemUpdateVesionInfoApi,
} from "@/api/device/index";
import { Toast } from "antd-mobile";

interface FormParams {
  serverUrl?: string;
}
export enum UpdateStatus {
  Check = "Check",
  NeedUpdate = "NeedUpdate", //有新版本需要更新
  NoNeedUpdate = "NoNeedUpdate", //不需要更新
  Updating = "Updating", //更新中
  UpdateDone = "UpdateDone", //更新完毕
}
interface IInitialState {
  status: UpdateStatus;
  visible: boolean;
  channelVisible: boolean;
  updateInfoVisible: boolean;
  percent: number;
  channel: DeviceChannleMode;
  address: string;
  downloadUrl: string;
}
type ActionType = { type: "setState"; payload: Partial<IInitialState> };
const initialState: IInitialState = {
  status: UpdateStatus.Check,
  visible: false,
  channelVisible: false,
  percent: 0,
  channel: DeviceChannleMode.Official,
  updateInfoVisible: false,
  address: "",
  downloadUrl: "",
};
function reducer(state: IInitialState, action: ActionType): IInitialState {
  switch (action.type) {
    case "setState":
      return {
        ...state,
        ...action.payload,
      };
    default:
      throw new Error();
  }
}

export function useData() {
  // ---状态管理
  const [state, stateDispatch] = useReducer<Reducer<IInitialState, ActionType>>(
    reducer,
    initialState
  );
  const { deviceInfo, updateDeviceInfo } = useConfigStore();
  const setStates = (payload: Partial<IInitialState>) => {
    stateDispatch({ type: "setState", payload: payload });
  };
  const addressFormRef = useRef<FormInstance>(null);

  const onQueryDeviceInfo = async () => {
    const res = await getDeviceInfoApi();
    updateDeviceInfo(res.data);
  };

  const onCancel = () => {
    setStates({
      visible: false,
    });
  };
  const resetFields = () => {
    addressFormRef.current?.resetFields();
  };

  const onUpdateCancel = async () => {
    await cancelUpdateApi();
    onCancleReset();
  };
  const onUpdateRestart = async () => {
    await setDevicePowerApi({ mode: PowerMode.Restart });
    Toast.show({
      icon: "success",
      content: "Operate Success",
    });
  };
  let timer: NodeJS.Timeout;
  const onCancleReset = () => {
    setStates({
      status: UpdateStatus.NeedUpdate,
    });
    clearInterval(timer);
  };
  const getProgress = async () => {
    setStates({
      status: UpdateStatus.Updating,
    });
    timer = setInterval(async () => {
      try {
        const { data } = await getUpdateSystemProgressApi();
        setStates({
          percent: data.progress,
        });
        if (data.progress >= 100) {
          setStates({
            status: UpdateStatus.UpdateDone,
          });
          clearInterval(timer);
        }
      } catch (err) {
        onCancleReset();
      }
    }, 1000);
  };
  const onUpdateApply = async () => {
    try {
      await updateSystemApi({
        downloadUrl: state.downloadUrl || "",
      });
      getProgress();
    } catch (err) {
      onCancleReset();
    }
  };
  const setStatus = (status: UpdateStatus, click?: boolean) => {
    if (click) {
      status = UpdateStatus.NoNeedUpdate;
      // 一分钟后才允许再次check
      const timer = setTimeout(() => {
        setStates({
          status: UpdateStatus.Check,
        });
        clearTimeout(timer);
      }, 60000);
    } else {
      status = UpdateStatus.Check;
    }
    setStates({
      status: status,
    });
  };
  const onUpdateCheck = async (click?: boolean) => {
    let status: UpdateStatus = UpdateStatus.Check;

    const url =
      deviceInfo.channel == DeviceChannleMode.Official
        ? deviceInfo.officialUrl
        : deviceInfo.serverUrl;
    if (!url) {
      // Toast.show({
      //   icon: 'fail',
      //   content: 'The server address cannot be empty',
      // })
      // 只有第一次使用时，没有url，重置为check
      setStates({
        status: UpdateStatus.Check,
      });
      return;
    }
    try {
      const { data } = await getSystemUpdateVesionInfoApi({
        channel: deviceInfo.channel,
        url: url,
      });
      if (data.isUpgrading === IsUpgrading.Yes) {
        getProgress();
        return;
      }
      if (!data.osName || !data.osVersion) {
        status = UpdateStatus.NoNeedUpdate;
        setStatus(status, click);
        return;
      }
      if (
        deviceInfo.osName != data.osName ||
        deviceInfo.osVersion != data.osVersion
      ) {
        status = UpdateStatus.NeedUpdate;
      } else {
        status = UpdateStatus.NoNeedUpdate;

        setStatus(status, click);
      }
      setStates({
        status: status,
        downloadUrl: data.downloadUrl + "/upgrade.zip",
      });
    } catch (err) {
      console.log(err, 999);
      status = UpdateStatus.NoNeedUpdate;
      setStatus(status, click);
    }
  };
  const onChannelChange = () => {
    setStates({
      channelVisible: true,
    });
  };
  const onUpdateChannelInfo = async (params: IChannelParams) => {
    await changeChannleApi(params);

    await onQueryDeviceInfo();

    setStates({
      visible: false,
      status: UpdateStatus.Check,
    });
    // onUpdateCheck()
  };

  const onEditServerAddress = () => {
    setStates({
      visible: true,
    });
  };

  const onConfirm = async (value: DeviceChannleMode[]) => {
    const params: IChannelParams = {
      channel: value[0],
      serverUrl: state.address,
    };

    onUpdateChannelInfo(params);
  };
  const onFinish = async (values: FormParams) => {
    await onUpdateChannelInfo({
      channel: state.channel,
      ...values,
    });
    resetFields();
  };

  useEffect(() => {
    if (deviceInfo) {
      setStates({
        channel: deviceInfo.channel,
        address: deviceInfo.serverUrl,
      });
      if (deviceInfo.needRestart == DeviceNeedRestart.Yes) {
        setStates({
          status: UpdateStatus.UpdateDone,
        });
      } else {
        onUpdateCheck();
      }
    }
  }, [deviceInfo]);

  return {
    state,
    setStates,
    deviceInfo,
    addressFormRef,
    onEditServerAddress,
    onCancel,
    onFinish,
    onUpdateCancel,
    onUpdateApply,
    onChannelChange,
    onConfirm,
    onUpdateRestart,
    onUpdateCheck,
  };
}
