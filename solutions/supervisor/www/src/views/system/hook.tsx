import { useEffect, useRef } from "react";
import { FormInstance } from "antd-mobile/es/components/form";
import useConfigStore from "@/store/config";
import {
  DeviceChannleMode,
  PowerMode,
  DeviceNeedRestart,
  SystemUpdateStatus,
  UpdateStatus,
} from "@/enum";
import { IChannelParams } from "@/api/device/device";
import {
  queryDeviceInfoApi,
  changeChannleApi,
  setDevicePowerApi,
  updateSystemApi,
  getUpdateSystemProgressApi,
  cancelUpdateApi,
  getSystemUpdateVesionInfoApi,
} from "@/api/device/index";
import { ReloadOutlined } from "@ant-design/icons";
import { message, Modal } from "antd";

interface FormParams {
  serverUrl?: string;
}

export function useData() {
  const {
    deviceInfo,
    systemUpdateState,
    updateDeviceInfo,
    setSystemUpdateState,
  } = useConfigStore();

  const addressFormRef = useRef<FormInstance>(null);
  const checkCountRef = useRef(0);
  const maxCheckCount = 10;

  const onQueryDeviceInfo = async () => {
    const res = await queryDeviceInfoApi();
    updateDeviceInfo(res.data);
  };

  const onCancel = () => {
    setSystemUpdateState({
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
    Modal.info({
      title: "Device rebooting....",
      icon: <ReloadOutlined spin style={{ color: "#8fc31f" }} />,
      content: "Please refresh the page in 5 seconds",
      centered: true,
      footer: null,
    });
  };

  let timer: NodeJS.Timeout;
  const onCancleReset = () => {
    setSystemUpdateState({
      status: UpdateStatus.NeedUpdate,
    });
    clearInterval(timer);
  };
  const getProgress = async () => {
    setSystemUpdateState({
      status: UpdateStatus.Updating,
    });
    timer = setInterval(async () => {
      try {
        const { data } = await getUpdateSystemProgressApi();
        setSystemUpdateState({
          percent: data.progress,
        });
        if (data.progress >= 100) {
          setSystemUpdateState({
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
      await updateSystemApi();
      getProgress();
    } catch (err) {
      onCancleReset();
    }
  };
  const setStatus = (status: UpdateStatus, click?: boolean) => {
    if (click) {
      status = UpdateStatus.NoNeedUpdate;
      // Wait one minute before allowing another check
      const timer = setTimeout(() => {
        setSystemUpdateState({
          status: UpdateStatus.Check,
        });
        clearTimeout(timer);
      }, 60000);
    } else {
      status = UpdateStatus.Check;
    }
    setSystemUpdateState({
      status: status,
    });
  };
  const onUpdateCheck = async (click?: boolean) => {
    // Reset check counter
    checkCountRef.current = 0;

    const performCheck = async () => {
      let status: UpdateStatus = UpdateStatus.Check;

      const url =
        deviceInfo.channel == DeviceChannleMode.Official
          ? deviceInfo.officialUrl
          : deviceInfo.serverUrl;
      if (!url) {
        // Only on first use when no url is set, reset to check
        setSystemUpdateState({
          status: UpdateStatus.Check,
        });
        return;
      }

      // Check if exceeded max check count
      if (checkCountRef.current >= maxCheckCount) {
        status = UpdateStatus.NoNeedUpdate;
        setStatus(status, click);
        return;
      }

      try {
        const { code, data } = await getSystemUpdateVesionInfoApi({
          channel: deviceInfo.channel,
          url: url,
        });

        checkCountRef.current++;

        if (code == -1) {
          message.warning(
            "Failed to get system version update from the server, please check the Beta Participation Channel"
          );
          return;
        }

        if (data.status === SystemUpdateStatus.Checking) {
          // If checking status, continue query after 5 seconds
          setTimeout(() => {
            performCheck();
          }, 5000);
          return;
        }

        if (data.status === SystemUpdateStatus.Updating) {
          // Updating status, get progress
          getProgress();
          return;
        }

        if (data.status === SystemUpdateStatus.Normal) {
          // Normal status, decide whether to update by version comparison
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

          setSystemUpdateState({
            status: status,
          });
          return;
        }
      } catch (err) {
        console.log(err);
        status = UpdateStatus.NoNeedUpdate;
        setStatus(status, click);
      }
    };

    // Start executing check
    await performCheck();
  };
  const onChannelChange = () => {
    setSystemUpdateState({
      channelVisible: true,
    });
  };
  const onUpdateChannelInfo = async (params: IChannelParams) => {
    await changeChannleApi(params);

    await onQueryDeviceInfo();

    setSystemUpdateState({
      visible: false,
      status: UpdateStatus.Check,
    });
    // onUpdateCheck()
  };

  const onEditServerAddress = () => {
    setSystemUpdateState({
      visible: true,
    });
  };

  const onConfirm = (value: DeviceChannleMode[]) => {
    const params: IChannelParams = {
      channel: value[0],
      serverUrl: systemUpdateState.address,
    };

    onUpdateChannelInfo(params);
  };
  const onFinish = async (values: FormParams) => {
    await onUpdateChannelInfo({
      channel: systemUpdateState.channel,
      ...values,
    });
    resetFields();
  };

  useEffect(() => {
    if (deviceInfo) {
      setSystemUpdateState({
        channel: deviceInfo.channel,
        address: deviceInfo.serverUrl,
      });
      if (deviceInfo.needRestart == DeviceNeedRestart.Yes) {
        setSystemUpdateState({
          status: UpdateStatus.UpdateDone,
        });
      } else {
        onUpdateCheck();
      }
    }
  }, [deviceInfo.channel, deviceInfo.serverUrl, deviceInfo.needRestart]);

  return {
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
