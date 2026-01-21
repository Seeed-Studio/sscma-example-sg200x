import { useEffect, Reducer, useReducer, useRef } from "react";
import type { FormInstance } from "antd/es/form";
import {
  getWiFiInfoListApi,
  switchWiFiApi,
  disconnectWifiApi,
  connectWifiApi,
  forgetWiFiApi,
  getHalowInfoListApi,
  switchHalowApi,
  switchAntennaApi,
  disconnectHalowApi,
  connectHalowApi,
  forgetHalowApi,
} from "@/api/network";
import {
  WifiConnectedStatus,
  NetworkStatus,
  WifiAuth,
  WifiEnable,
  HalowEnable,
  AntennaEnable,
} from "@/enum/network";
import {
  IWifiInfo,
  IConnectParams,
  IConnectHalowParams,
  IHalowInfo,
} from "@/api/network/network";
import useConfigStore from "@/store/config";

interface FormParams {
  password?: string;
}

const timeGap = 5000;
let timer: NodeJS.Timeout | null = null;
let halowTimer: NodeJS.Timeout | null = null;

export enum OperateType {
  Connect = "Connect",
  DisConnect = "DisConnect",
  Forget = "Forget",
}
export enum FormType {
  Password = "Password",
  Disabled = "Disabled",
  HalowConfig = "HalowConfig",
}

interface IInitialState {
  queryNum: number;
  wifiStatus: NetworkStatus;
  wifiChecked: boolean;
  // 0: 关闭, 1: 开启, 2: 不可用
  wifiEnable?: WifiEnable;
  wifiVisible: boolean;
  visible: boolean;
  password: string;
  channel: string;
  // 直接使用接口返回的数据，不需要重新分类
  etherInfo?: IWifiInfo;
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
  // Halow 相关状态
  activeTab: "wifi" | "halow";
  halowChecked: boolean;
  halowEnable?: HalowEnable;
  antennaEnable?: AntennaEnable; // 天线状态
  connectedHalowInfoList: IWifiInfo[];
  halowInfoList: IWifiInfo[];
  selectedHalowInfo?: IWifiInfo;
  halowTimer: NodeJS.Timeout | null;
  halowConfig?: IHalowInfo; // Halow 配置信息
  halowCountry?: "AU" | "EU" | "IN" | "JP" | "KR" | "NZ" | "SG" | "US"; // Halow 国家设置
}
type ActionType = { type: "setState"; payload: Partial<IInitialState> };
const initialState: IInitialState = {
  queryNum: 0,
  wifiStatus: NetworkStatus.Disconnected,
  wifiChecked: false,
  wifiEnable: WifiEnable.Close,
  wifiVisible: false,
  visible: false,
  password: "",
  channel: "",
  etherInfo: undefined,
  connectedWifiInfoList: [],
  wifiInfoList: [],
  selectedWifiInfo: undefined,
  timer: null,
  formType: FormType.Disabled,
  submitLoading: false,
  needRefresh: true,
  etherStatus: NetworkStatus.Disconnected,
  connectLoading: false,
  activeTab: "wifi",
  halowChecked: false,
  halowEnable: HalowEnable.Close,
  antennaEnable: AntennaEnable.RF1,
  connectedHalowInfoList: [],
  halowInfoList: [],
  selectedHalowInfo: undefined,
  halowTimer: null,
  halowConfig: undefined,
  halowCountry: "US",
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
  const passwordFormRef = useRef<FormInstance | null>(null);
  const prevHalowCheckedRef = useRef<boolean | undefined>(undefined);
  const isTabSwitchingRef = useRef<boolean>(false);

  const getWifiResults = async () => {
    const { data } = await getWiFiInfoListApi();

    setStates({
      // wifiStatus 由是否有已连接的条目推导
      wifiStatus: (data.connectedWifiInfoList || []).some(
        (item) => item.status === NetworkStatus.Connected
      )
        ? NetworkStatus.Connected
        : NetworkStatus.Disconnected,
      wifiEnable: data.wifiEnable,
      wifiChecked: data.wifiEnable === 1,
      connectedWifiInfoList: data.connectedWifiInfoList || [],
      wifiInfoList: data.wifiInfoList || [],
      etherInfo: data.etherInfo,
      etherStatus: data.etherInfo
        ? data.etherInfo.status
        : NetworkStatus.Disconnected,
    });
  };

  const getWifiList = async () => {
    try {
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
    // wifi没有开启或不在wifi tab时不需要开启自动刷新
    if (!state.wifiChecked || state.activeTab !== "wifi") return;
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
      submitLoading: false,
      submitType: undefined,
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
      submitLoading: false,
      submitType: undefined,
    });
  };
  const onClickWifiInfo = (wifiItem: IWifiInfo) => {
    onShowWifiItemInfo(wifiItem);
  };
  const onClickWifiItem = (wifiItem: IWifiInfo) => {
    if (state.connectLoading) {
      return;
    }
    // 先记录选中项
    setStates({
      selectedWifiInfo: wifiItem,
    });
    // 当前已连接的 WiFi：直接展示详情（IP 信息等），而不是再次发起连接
    if (wifiItem.status === NetworkStatus.Connected) {
      onShowWifiItemInfo(wifiItem);
      return;
    }
    // 其它 WiFi：走连接流程（开放/已保存直接连，需要密码弹出密码框）
    onHandleConnect(wifiItem);
  };

  const onClickEthernetItem = () => {
    if (state.etherInfo) {
      onShowWifiItemInfo(state.etherInfo);
    }
  };
  // 视觉优化：将目标 WiFi 标记为 Connecting，并将其他已连接项标记为 Disconnected
  const setConnectingVisualState = (targetSsid: string) => {
    const normalizeList = (list: IWifiInfo[]) =>
      list.map((item) => {
        if (item.ssid === targetSsid) {
          return { ...item, status: NetworkStatus.Connecting };
        }
        // 清除除目标项外的所有状态（包括已连接和连接中），统一变为未连接
        if (
          item.status === NetworkStatus.Connected ||
          item.status === NetworkStatus.Connecting
        ) {
          return { ...item, status: NetworkStatus.Disconnected };
        }
        return item;
      });

    setStates({
      connectedWifiInfoList: normalizeList(state.connectedWifiInfoList),
      wifiInfoList: normalizeList(state.wifiInfoList),
      selectedWifiInfo:
        state.selectedWifiInfo?.ssid === targetSsid
          ? { ...state.selectedWifiInfo, status: NetworkStatus.Connecting }
          : state.selectedWifiInfo &&
            (state.selectedWifiInfo.status === NetworkStatus.Connected ||
              state.selectedWifiInfo.status === NetworkStatus.Connecting)
          ? { ...state.selectedWifiInfo, status: NetworkStatus.Disconnected }
          : state.selectedWifiInfo,
    });
  };
  const onHandleConnect = async (targetWifi?: IWifiInfo) => {
    try {
      const selected = targetWifi || state.selectedWifiInfo;
      // 以太网或缺少 ssid 的项不触发连接
      if (!selected || !selected.ssid || selected === state.etherInfo) return;
      const isSavedNetwork = selected
        ? state.connectedWifiInfoList.some(
            (item) => item.ssid === selected.ssid
          )
        : false;

      // 已连接过、已保存到“我的网络”的WiFi，或无加密WiFi，均直接连接且不传密码
      if (
        selected.connectedStatus === WifiConnectedStatus.Yes ||
        selected.auth === WifiAuth.NoNeed ||
        isSavedNetwork
      ) {
        // 切换视觉：目标项 Loading，其他已连接项去勾
        setConnectingVisualState(selected.ssid);
        setStates({
          connectLoading: true,
        });
        await onConnect(undefined, selected.ssid);
      } else if (selected.auth === WifiAuth.Need) {
        // 需要密码的WiFi，显示密码输入框
        setStates({
          visible: true,
          formType: FormType.Password,
        });
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
      const targetSsid = ssid || state.selectedWifiInfo?.ssid || "";
      // 防御：ssid 为空或当前为以太网则不请求
      if (!targetSsid || state.selectedWifiInfo === state.etherInfo) {
        return;
      }
      const params: IConnectParams = {
        ssid: targetSsid,
      };
      if (values?.password) {
        params.password = values.password;
      }
      setStates({
        submitLoading: true,
      });
      // 视觉：提交连接时开始 Loading，仅目标项 Loading，其他已连接项去勾
      setConnectingVisualState(targetSsid);

      await connectWifiApi(params);
      setStates({
        wifiVisible: false,
        visible: false,
        submitLoading: false,
        submitType: undefined,
      });
      // 延迟刷新以获取较稳定的连接结果（避免立即重复请求）
      setTimeout(() => {
        setStates({
          needRefresh: true,
        });
      }, 1000);
      resetFields();
    } catch (err) {}
    setStates({
      submitLoading: false,
      submitType: undefined,
    });
  };
  // 刷新wifi状态
  const refreshWifiStatus = async () => {
    const { data } = await getWiFiInfoListApi();

    const wifiStatus = (data.connectedWifiInfoList || []).some(
      (item) => item.status === NetworkStatus.Connected
    )
      ? NetworkStatus.Connected
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
    // 防御：以太网或缺少 ssid 的项不触发任何 WiFi 管理接口
    if (!info.ssid || info === state.etherInfo) return;
    setStates({
      submitType: type,
    });
    try {
      switch (type) {
        case OperateType.Connect:
          onHandleConnect();
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
            submitLoading: false,
            submitType: undefined,
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
            submitLoading: false,
            submitType: undefined,
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
    // WiFi 开关状态变化时的处理
    if (state.activeTab === "wifi") {
      if (state.wifiChecked) {
        getWifiResults();
        onAutoRefreshWifiList();
      } else {
        onStopRefreshWifiList();
      }
    }
    setStates({
      submitLoading: false,
    });
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [state.wifiChecked]);

  useEffect(() => {
    if (state.needRefresh) {
      if (state.activeTab === "wifi") {
        onQueryData();
      } else {
        getHalowResults();
      }
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [state.needRefresh, state.activeTab]);
  // Halow 相关函数
  const getHalowResults = async () => {
    const { data } = await getHalowInfoListApi();
    setStates({
      halowEnable: data.halowEnable,
      halowChecked: data.halowEnable === 1,
      antennaEnable: data.antennaEnable,
      connectedHalowInfoList: data.connectedHalowInfoList || [],
      halowInfoList: data.halowInfoList || [],
    });
  };

  const getHalowList = async () => {
    try {
      if (!state.halowChecked) return;
      await getHalowResults();
      return true;
    } catch (err) {}
  };

  const onStopRefreshHalowList = () => {
    halowTimer && clearInterval(halowTimer);
  };

  const onAutoRefreshHalowList = async () => {
    onStopRefreshHalowList();
    // halow没有开启或不在halow tab时不需要开启自动刷新
    if (!state.halowChecked || state.activeTab !== "halow") return;
    halowTimer = setInterval(() => {
      getHalowList();
    }, timeGap);
  };

  const handleSwitchHalow = async () => {
    await switchHalowApi({ mode: Number(!state.halowChecked) as HalowEnable });
    setStates({
      halowChecked: !state.halowChecked,
      visible: false,
    });
  };

  const handleSwitchAntenna = async () => {
    const newMode =
      state.antennaEnable === AntennaEnable.RF1
        ? AntennaEnable.RF2
        : AntennaEnable.RF1;
    await switchAntennaApi({ mode: newMode });
    setStates({
      antennaEnable: newMode,
    });
  };

  const onSwitchEnabledHalow = async () => {
    if (state.halowChecked) {
      setStates({
        visible: true,
        formType: FormType.Disabled,
      });
    } else {
      handleSwitchHalow();
    }
  };

  const onShowHalowItemInfo = (item: IWifiInfo) => {
    setStates({
      selectedHalowInfo: item,
      wifiVisible: true,
      submitLoading: false,
      submitType: undefined,
    });
  };

  const onClickHalowInfo = (halowItem: IWifiInfo) => {
    onShowHalowItemInfo(halowItem);
  };

  // 直接连接 Halow（不弹窗，用于重连已保存的网络）
  const onConnectHalowDirect = async (ssid: string) => {
    try {
      if (!ssid) {
        return;
      }
      setStates({
        connectLoading: true,
      });
      setConnectingHalowVisualState(ssid);

      // 直接连接，只传 ssid，后端会根据已保存的网络信息自动使用之前的配置
      const params: IConnectHalowParams = {
        ssid: ssid,
      };

      await connectHalowApi(params);
      setTimeout(() => {
        setStates({
          needRefresh: true,
          connectLoading: false,
        });
      }, 1000);
    } catch (err) {
      setStates({
        connectLoading: false,
      });
    }
  };

  const onClickHalowItem = (halowItem: IWifiInfo) => {
    if (state.connectLoading) {
      return;
    }
    setStates({
      selectedHalowInfo: halowItem,
    });
    if (halowItem.status === NetworkStatus.Connected) {
      onShowHalowItemInfo(halowItem);
      return;
    }
    // 如果是已连接过的网络（在 My Networks 中），直接连接，不弹窗
    const isConnectedNetwork = state.connectedHalowInfoList.some(
      (item) => item.ssid === halowItem.ssid
    );
    if (isConnectedNetwork) {
      // 直接连接，只传 ssid
      onConnectHalowDirect(halowItem.ssid);
      return;
    }
    // 如果是未连接过的网络，弹出配置表单
    onHandleConnectHalow(halowItem);
  };

  const setConnectingHalowVisualState = (targetSsid: string) => {
    const normalizeList = (list: IWifiInfo[]) =>
      list.map((item) => {
        if (item.ssid === targetSsid) {
          return { ...item, status: NetworkStatus.Connecting };
        }
        if (
          item.status === NetworkStatus.Connected ||
          item.status === NetworkStatus.Connecting
        ) {
          return { ...item, status: NetworkStatus.Disconnected };
        }
        return item;
      });

    setStates({
      connectedHalowInfoList: normalizeList(state.connectedHalowInfoList),
      halowInfoList: normalizeList(state.halowInfoList),
      selectedHalowInfo:
        state.selectedHalowInfo?.ssid === targetSsid
          ? { ...state.selectedHalowInfo, status: NetworkStatus.Connecting }
          : state.selectedHalowInfo &&
            (state.selectedHalowInfo.status === NetworkStatus.Connected ||
              state.selectedHalowInfo.status === NetworkStatus.Connecting)
          ? { ...state.selectedHalowInfo, status: NetworkStatus.Disconnected }
          : state.selectedHalowInfo,
    });
  };

  const onHandleConnectHalow = async (targetHalow?: IWifiInfo) => {
    try {
      const selected = targetHalow || state.selectedHalowInfo;
      if (!selected || !selected.ssid) return;

      // Halow 连接总是需要配置信息，先弹出配置表单
      // 如果需要密码，会在配置表单提交后再处理
      setStates({
        visible: true,
        formType: FormType.HalowConfig,
        selectedHalowInfo: selected,
      });
    } catch (err) {
    } finally {
      setStates({
        connectLoading: false,
      });
    }
  };

  const onOpenManualHalowConfig = () => {
    setStates({
      visible: true,
      formType: FormType.HalowConfig,
      selectedHalowInfo: undefined,
    });
  };

  const onConnectHalow = async (
    values?: FormParams & { halowConfig?: IHalowInfo },
    ssid?: string
  ) => {
    try {
      const targetSsid = ssid || state.selectedHalowInfo?.ssid || "";
      if (!targetSsid) {
        return;
      }
      // 使用用户输入的配置信息或 state 中保存的配置
      const halowConfig = values?.halowConfig || state.halowConfig;
      if (!halowConfig) {
        return;
      }

      const params: IConnectHalowParams = {
        ssid: targetSsid,
        halowInfo: halowConfig,
      };
      if (values?.password) {
        params.password = values.password;
      }
      setStates({
        submitLoading: true,
      });
      setConnectingHalowVisualState(targetSsid);

      await connectHalowApi(params);
      setStates({
        wifiVisible: false,
        visible: false,
        submitLoading: false,
        submitType: undefined,
        halowConfig: undefined,
      });
      setTimeout(() => {
        setStates({
          needRefresh: true,
        });
      }, 1000);
      resetFields();
    } catch (err) {}
    setStates({
      submitLoading: false,
      submitType: undefined,
    });
  };

  const onHandleHalowOperate = async (type: OperateType) => {
    const info = state.selectedHalowInfo;
    if (!info || !info.ssid) return;
    setStates({
      submitType: type,
    });
    try {
      switch (type) {
        case OperateType.Connect:
          onHandleConnectHalow();
          break;
        case OperateType.DisConnect:
          setStates({
            submitLoading: true,
          });
          await disconnectHalowApi({
            ssid: info.ssid || "",
          });
          setStates({
            wifiVisible: false,
            needRefresh: true,
            submitLoading: false,
            submitType: undefined,
          });
          break;
        case OperateType.Forget:
          setStates({
            submitLoading: true,
          });
          await forgetHalowApi({
            ssid: info.ssid || "",
          });
          setStates({
            wifiVisible: false,
            needRefresh: true,
            submitLoading: false,
            submitType: undefined,
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
    // Halow 开关状态变化时的处理
    // 只有当 halowChecked 从 false 变为 true 时才请求列表（用户手动打开开关）
    // Tab 切换时的列表请求由 Tab 切换的 useEffect 处理
    if (state.activeTab === "halow") {
      const prevChecked = prevHalowCheckedRef.current;
      const currentChecked = state.halowChecked;

      // 只有当从 false 变为 true 时才请求列表（用户手动打开开关）
      if (
        currentChecked &&
        prevChecked === false &&
        !isTabSwitchingRef.current
      ) {
        getHalowResults();
      }

      if (currentChecked) {
        onAutoRefreshHalowList();
      } else {
        onStopRefreshHalowList();
      }

      prevHalowCheckedRef.current = currentChecked;
      isTabSwitchingRef.current = false;
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [state.halowChecked]);

  useEffect(() => {
    // 切换 Tab 时停止另一个 Tab 的定时器，启动当前 Tab 的定时器，并请求对应列表
    if (state.activeTab === "wifi") {
      onStopRefreshHalowList();
      // 切换到 WiFi tab 时，无论 WiFi 是否开启，都请求一次以获取状态
      getWifiResults();
      if (state.wifiChecked) {
        onAutoRefreshWifiList();
      }
    } else if (state.activeTab === "halow") {
      onStopRefreshWifiList();
      // 切换到 Halow tab 时，无论 Halow 是否开启，都请求一次以获取状态
      // 标记正在 Tab 切换，避免 halowChecked 的 useEffect 重复请求
      isTabSwitchingRef.current = true;
      getHalowResults();
      if (state.halowChecked) {
        onAutoRefreshHalowList();
      }
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [state.activeTab]);

  useEffect(() => {
    // 清除定时器
    return () => {
      onStopRefreshWifiList();
      onStopRefreshHalowList();
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
    onClickWifiInfo,
    onClickEthernetItem,
    handleSwitchWifi,
    onSwitchEnabledHalow,
    handleSwitchHalow,
    handleSwitchAntenna,
    onClickHalowItem,
    onClickHalowInfo,
    onHandleHalowOperate,
    onConnectHalow,
    onOpenManualHalowConfig,
  };
}
