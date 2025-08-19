import { useEffect, Reducer, useReducer, useRef } from "react";
import { FormInstance } from "antd-mobile/es/components/form";
import {
  getWiFiInfoListApi,
  switchWiFiApi,
  disconnectWifiApi,
  connectWifiApi,
  autoConnectWiFiApi,
  forgetWiFiApi,
} from "@/api/network";
import {
  WifiConnectedStatus,
  NetworkStatus,
  WifiAuth,
} from "@/enum/network";
import { IWifiInfo, IConnectParams } from "@/api/network/network";
import useConfigStore from "@/store/config";

interface FormParams {
  password?: string;
}

const timeGap = 5000;
let timer: NodeJS.Timeout | null = null;

export enum OperateType {
  Connect = "Connect",
  DisConnect = "DisConnect",
  Forget = "Forget",
  AutoConnect = "AutoConnect",
}
export enum FormType {
  Password = "Password",
  Disabled = "Disabled",
}

interface IInitialState {
  queryNum: number;
  wifiStatus: NetworkStatus;
  wifiChecked: boolean;
  wifiVisible: boolean;
  visible: boolean;
  passwordVisible: boolean;
  password: string;
  channel: string;
  // 直接使用接口返回的数据，不需要重新分类
  etherInfo?: IWifiInfo;
  currentWifiInfo?: IWifiInfo;
  connectedWifiInfoList: IWifiInfo[];
  wifiInfoList: IWifiInfo[];
  selectedWifiInfo?: IWifiInfo; // 用户选中的WiFi项，用于显示详情和操作
  timer: NodeJS.Timeout | null;
  formType: FormType;
  submitLoading: boolean;
  submitType?: OperateType;
  needRefresh: boolean;
  etherStatus: NetworkStatus;
  connectLoading: boolean;
}
type ActionType = { type: "setState"; payload: Partial<IInitialState> };
const initialState: IInitialState = {
  queryNum: 0,
  wifiStatus: NetworkStatus.Disconnected,
  wifiChecked: false,
  wifiVisible: false,
  visible: false,
  password: "",
  channel: "",
  etherInfo: undefined,
  currentWifiInfo: undefined,
  connectedWifiInfoList: [],
  wifiInfoList: [],
  selectedWifiInfo: undefined,
  timer: null,
  formType: FormType.Disabled,
  submitLoading: false,
  needRefresh: true,
  etherStatus: NetworkStatus.Disconnected,
  passwordVisible: false,
  connectLoading: false,
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
  const { updateWifiStatus, updateEtherStatus } = useConfigStore();

  // ---状态管理
  const [state, stateDispatch] = useReducer<Reducer<IInitialState, ActionType>>(
    reducer,
    initialState
  );
  const setStates = (payload: Partial<IInitialState>) => {
    stateDispatch({ type: "setState", payload: payload });
  };
  const passwordFormRef = useRef<FormInstance>(null);



  const getWifiResults = async () => {
    const { data } = await getWiFiInfoListApi();

    setStates({
      wifiStatus: data.currentWifiInfo
        ? data.currentWifiInfo.status
        : NetworkStatus.Disconnected,
      wifiChecked: data.wifiEnable === 1,
      currentWifiInfo: data.currentWifiInfo,
      connectedWifiInfoList: data.connectedWifiInfoList || [],
      wifiInfoList: data.wifiInfoList || [],
      etherInfo: data.etherInfo,
      etherStatus: data.etherInfo ? data.etherInfo.status : NetworkStatus.Disconnected,
    });
  };

  const getWifiList = async () => {
    try {
      if (state.visible || state.wifiVisible) return;
      // wifi没有开启不需要开启自动刷新
      if (!state.wifiChecked) return;
      await getWifiResults();
      return true;
    } catch (err) {}
  };

  //
  const onStopRefreshWifiList = () => {
    // state.timer && clearInterval(state.timer)
    timer && clearInterval(timer);
  };

  // 自动刷新wifilist
  const onAutoRefreshWifiList = async () => {
    onStopRefreshWifiList();
    // wifi没有开启不需要开启自动刷新
    if (!state.wifiChecked) return;
    timer = setInterval(() => {
      getWifiList();
    }, timeGap);
  };
  const handleSwitchWifi = async () => {
    await switchWiFiApi({ mode: Number(!state.wifiChecked) });
    setStates({
      wifiChecked: !state.wifiChecked,
      visible: false,
    });
  };
  const onSwitchEnabledWifi = async () => {
    if (state.wifiChecked) {
      setStates({
        visible: true,
        formType: FormType.Disabled,
      });
    } else {
      handleSwitchWifi();
    }
  };
  const toggleVisible = () => {
    setStates({
      visible: !state.visible,
    });
  };

  const resetFields = () => {
    passwordFormRef.current?.resetFields();
  };
  const onQueryData = async () => {
    try {
      // 直接调用getWifiResults，它已经包含了所有需要的数据和状态更新
      await getWifiResults();
    } catch (err) {}
    setStates({
      needRefresh: false,
    });
  };

  const onShowWifiItemInfo = (item: IWifiInfo) => {
    setStates({
      selectedWifiInfo: item,
      wifiVisible: true,
    });
  };
  const onClickWifiItem = (
    wifiItem: IWifiInfo,
    showInfo?: boolean
  ) => {
    onStopRefreshWifiList();
    if (
      showInfo ||
      wifiItem.type === 1 || // 有线网络
      wifiItem.ssid == state.currentWifiInfo?.ssid
    ) {
      onShowWifiItemInfo(wifiItem);
    } else {
      if (state.connectLoading) {
        return;
      }
      // 先设置选中的WiFi项，再执行连接操作
      setStates({
        selectedWifiInfo: wifiItem,
      });
      // 根据WiFi项的类型和状态来决定操作
      if (wifiItem.connectedStatus === WifiConnectedStatus.Yes) {
        // 已连接的WiFi，直接操作
        onHandleConnect();
      } else {
        // 未连接的WiFi，需要密码
        onHandleConnect();
      }
    }
  };
  const onHandleConnect = async () => {
    try {
      if (state.selectedWifiInfo?.connectedStatus === WifiConnectedStatus.Yes) {
        // 已连接过的直接连接
        setStates({
          connectLoading: true,
        });
        await onConnect(undefined, state.selectedWifiInfo.ssid);
      } else if (state.selectedWifiInfo?.auth === WifiAuth.Need) {
        // 需要密码的WiFi，显示密码输入框
        setStates({
          visible: true,
          formType: FormType.Password,
        });
      } else if (state.selectedWifiInfo) {
        // 不需要密码的WiFi，直接连接
        setStates({
          connectLoading: true,
        });
        await onConnect(undefined, state.selectedWifiInfo.ssid);
      }
    } catch (err) {
      // 错误处理
    } finally {
      // 确保无论成功还是失败都重置loading状态
      setStates({
        connectLoading: false,
      });
    }
  };

  const onConnect = async (values?: FormParams, ssid?: string) => {
    try {
      console.log("connectWifiApi values", values);
      console.log("connectWifiApi ssid", ssid);
      console.log("connectWifiApi state.selectedWifiInfo", state.selectedWifiInfo);
      const params: IConnectParams = {
        ssid: ssid || state.selectedWifiInfo?.ssid || "",
      };
      values && (params.password = values?.password || "");
      console.log("connectWifiApi params", params);
      setStates({
        submitLoading: true,
      });
      
      // 立即更新选中的WiFi项状态为连接中
      if (state.selectedWifiInfo) {
        const updatedWifiInfo = {
          ...state.selectedWifiInfo,
          status: NetworkStatus.Connecting
        };
        setStates({
          selectedWifiInfo: updatedWifiInfo
        });
        
        // 同时更新列表中的对应项
        const updateWifiItemStatus = (list: IWifiInfo[], ssid: string, status: NetworkStatus) => {
          return list.map(item => 
            item.ssid === ssid ? { ...item, status } : item
          );
        };
        
        const currentSsid = ssid || state.selectedWifiInfo.ssid;
        if (currentSsid) {
          setStates({
            connectedWifiInfoList: updateWifiItemStatus(state.connectedWifiInfoList, currentSsid, NetworkStatus.Connecting),
            wifiInfoList: updateWifiItemStatus(state.wifiInfoList, currentSsid, NetworkStatus.Connecting)
          });
        }
      }
      
      await connectWifiApi(params);
      setStates({
        wifiVisible: false,
        visible: false,
      });
      // 立即刷新WiFi列表以获取最新的status
      await getWifiResults();
      // 延迟刷新以获取最终状态
      setTimeout(() => {
        setStates({
          needRefresh: true,
        });
      }, 2000);
      resetFields();
    } catch (err) {}
    setStates({
      submitLoading: false,
    });
  };
  // 刷新wifi状态
  const refreshWifiStatus = async () => {
    const { data } = await getWiFiInfoListApi();

    const wifiStatus = data.currentWifiInfo
      ? data.currentWifiInfo.status
      : NetworkStatus.Disconnected;

    updateWifiStatus(wifiStatus);

    const etherStatus = data.etherInfo
      ? data.etherInfo.status
      : NetworkStatus.Disconnected;

    updateEtherStatus(etherStatus);
  };
  const onHandleOperate = async (type: OperateType) => {
    const info = state.selectedWifiInfo;
    if (!info) return;
    setStates({
      submitType: type,
    });
    try {
      switch (type) {
        case OperateType.Connect:
          onHandleConnect();
          break;
        case OperateType.AutoConnect:
          setStates({
            submitLoading: true,
          });
          await autoConnectWiFiApi({
            ssid: info.ssid || "",
            mode: Number(!info.autoConnect),
          });
          refreshWifiStatus();
          setStates({
            submitLoading: false,
            selectedWifiInfo: state.selectedWifiInfo ? {
              ...state.selectedWifiInfo,
              autoConnect: Number(!state.selectedWifiInfo.autoConnect),
              connectChecked: !state.selectedWifiInfo.connectChecked,
            } : undefined,
            wifiVisible: true,
          });
          getWifiList();
          break;
        case OperateType.DisConnect:
          setStates({
            submitLoading: true,
          });
          await disconnectWifiApi({
            ssid: info.ssid || "",
          });
          refreshWifiStatus();

          setStates({
            wifiVisible: false,
            needRefresh: true,
          });
          break;
        case OperateType.Forget:
          setStates({
            submitLoading: true,
          });
          await forgetWiFiApi({
            ssid: info.ssid || "",
          });
          setStates({
            wifiVisible: false,
            needRefresh: true,
          });
          break;
        default:
          throw new Error();
      }
    } catch (err) {
      setStates({
        submitLoading: false,
      });
    }
  };

  useEffect(() => {
    if (state.wifiChecked && !state.wifiVisible && !state.visible) {
      getWifiResults();
      onAutoRefreshWifiList();
    } else {
      onStopRefreshWifiList();
    }
    setStates({
      submitLoading: false,
    });
  }, [state.wifiChecked, state.wifiVisible, state.visible]);

  useEffect(() => {
    if (state.needRefresh) {
      onQueryData();
    }
  }, [state.needRefresh]);
  useEffect(() => {
    // 清除定时器
    return () => {
      onStopRefreshWifiList();
    };
  }, []);
  return {
    state,
    passwordFormRef,
    setStates,
    onSwitchEnabledWifi,
    toggleVisible,
    onConnect,
    onHandleOperate,
    onClickWifiItem,
    handleSwitchWifi,
  };
}
