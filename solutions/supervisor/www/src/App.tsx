import { useEffect, useMemo } from "react";
import { ConfigProvider } from "antd";
import { ConfigProvider as MobileConfigProvider } from "antd-mobile";
import enUS from "antd-mobile/es/locales/en-US";
import { createHashRouter, RouterProvider } from "react-router-dom";
import Routes from "@/router";
import Login from "@/views/login";
import { queryDeviceInfoApi } from "@/api/device/index";
import { getUserInfoApi } from "@/api/user";
import useUserStore from "@/store/user";
import useConfigStore from "@/store/config";
import { Version } from "@/utils";

const router = createHashRouter(Routes);

const App = () => {
  const {
    currentSn,
    usersBySn,
    setCurrentSn,
    updateFirstLogin,
    clearCurrentUserInfo,
  } = useUserStore();

  const { updateDeviceInfo } = useConfigStore();

  useEffect(() => {
    console.log(`%cVersion: ${Version}`, "font-weight: bold");
    initUserData();
  }, []);

  const token = useMemo(() => {
    return currentSn ? usersBySn[currentSn]?.token : null;
  }, [usersBySn, currentSn]);

  const initUserData = async () => {
    try {
      const response = await queryDeviceInfoApi();
      const deviceInfo = response.data;
      // Query device info, get sn
      updateDeviceInfo(deviceInfo);
      const sn = deviceInfo.sn;
      setCurrentSn(sn);
      if (sn) {
        // Check if device is first login, if first login, go to login page
        const response = await getUserInfoApi();
        if (response.code == 0) {
          const data = response.data;
          const firstLogin = data.firstLogin;
          updateFirstLogin(firstLogin);
          if (firstLogin) {
            clearCurrentUserInfo();
          }
        }
      }
    } catch (error) {
      // Don't clear user info, likely service not started timeout
    }
  };

  return (
    <ConfigProvider
      theme={{
        token: {
          colorPrimary: "#8FC31F",
          colorPrimaryHover: "#81AE1B",
        },
      }}
    >
      <MobileConfigProvider locale={enUS}>
        <div className="w-full h-full">
          {token ? <RouterProvider router={router} /> : <Login />}
        </div>
      </MobileConfigProvider>
    </ConfigProvider>
  );
};

export default App;
