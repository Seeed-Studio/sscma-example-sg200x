import { create } from "zustand";
import { IDeviceInfo } from "@/api/device/device";
import { WifiConnectStatus } from "@/enum/network";

type ConfigStoreType = {
  deviceInfo: Partial<IDeviceInfo>;
  wifiStatus: WifiConnectStatus | undefined;
  updateDeviceInfo: (deviceInfo: IDeviceInfo) => void;
  updateWifiStatus: (wifiStatus: WifiConnectStatus) => void;
};

const useConfigStore = create<ConfigStoreType>((set) => ({
  deviceInfo: {},
  wifiStatus: undefined,
  updateDeviceInfo: (deviceInfo: IDeviceInfo) => set(() => ({ deviceInfo })),
  updateWifiStatus: (wifiStatus: WifiConnectStatus) =>
    set(() => ({ wifiStatus })),
}));

export default useConfigStore;
