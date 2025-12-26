import { create } from "zustand";
import { getPlatformInfoApi, savePlatformInfoApi } from "@/api/device";

type PlatformStoreType = {
  nickname: string | null;
  updateNickname: (name: string) => void;
  clearUserInfo: () => void;
};

// Platform
const usePlatformStore = create<PlatformStoreType>((set) => ({
  nickname: null,
  updateNickname: (name: string) => set(() => ({ nickname: name })),
  clearUserInfo: () =>
    set(() => ({
      nickname: null,
    })),
}));

export const savePlatformInfo = async () => {
  try {
    const { nickname } = usePlatformStore.getState();
    const platform_info = {
      nickname,
    };
    await savePlatformInfoApi({ platform_info: JSON.stringify(platform_info) });
  } catch (error) {
    console.log(error);
  }
};

export const initPlatformStore = async () => {
  try {
    const resp = await getPlatformInfoApi();
    if (resp.code == 0) {
      const data = resp.data;
      const platform_info = data?.platform_info;
      if (platform_info) {
        const platformInfo = JSON.parse(platform_info);
        usePlatformStore.setState({
          nickname: platformInfo.nickname || null,
        });
      }
    }
  } catch (error) {
    console.log(error);
  }
};

export const logout = async () => {
  const { clearUserInfo } = usePlatformStore.getState();
  clearUserInfo();
  savePlatformInfo();
};

export default usePlatformStore;
