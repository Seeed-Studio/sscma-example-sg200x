import { create } from "zustand";
import { IDeviceInfo } from "@/api/device/device";
import { NetworkStatus } from "@/enum/network";
import { DeviceChannleMode, UpdateStatus } from "@/enum";

interface SystemUpdateState {
  status: UpdateStatus;
  visible: boolean;
  channelVisible: boolean;
  updateInfoVisible: boolean;
  percent: number;
  channel: DeviceChannleMode;
  address: string;
}

type ConfigStoreType = {
  deviceInfo: Partial<IDeviceInfo>;
  wifiStatus: NetworkStatus | undefined;
  etherStatus: NetworkStatus | undefined;
  systemUpdateState: Partial<SystemUpdateState>;
  updateDeviceInfo: (deviceInfo: IDeviceInfo) => void;
  updateWifiStatus: (wifiStatus: NetworkStatus) => void;
  updateEtherStatus: (etherStatus: NetworkStatus) => void;
  setSystemUpdateState: (payload: Partial<SystemUpdateState>) => void;
};

const useConfigStore = create<ConfigStoreType>((set) => ({
  deviceInfo: {},
  wifiStatus: undefined,
  etherStatus: undefined,
  systemUpdateState: {
    status: UpdateStatus.Check,
    visible: false,
    channelVisible: false,
    percent: 0,
    channel: DeviceChannleMode.Official,
    updateInfoVisible: false,
    address: "",
  },
  updateDeviceInfo: (deviceInfo: IDeviceInfo) => set(() => ({ deviceInfo })),
  updateWifiStatus: (wifiStatus: NetworkStatus) => set(() => ({ wifiStatus })),
  updateEtherStatus: (etherStatus: NetworkStatus) =>
    set(() => ({ etherStatus })),
  setSystemUpdateState: (payload) =>
    set((state) => ({
      systemUpdateState: { ...state.systemUpdateState, ...payload },
    })),
}));

export default useConfigStore;
