import { useEffect, useMemo } from "react";
import { createHashRouter, RouterProvider } from "react-router-dom";
import Routes from "@/router";
import Login from "@/views/login";
import { ConfigProvider } from "antd";
import useUserStore, { getUserInfoBySN } from "@/store/user";
import { getDeviceInfoApi } from "@/api/device";
import { getUserInfoApi, loginApi } from "@/api/user";

const router = createHashRouter(Routes);

const App = () => {
  const {
    currentSn,
    usersBySn,
    setCurrentSn,
    updateFirstLogin,
    updateToken,
    clearCurrentUserInfo,
  } = useUserStore();

  useEffect(() => {
    initUserData();
  }, []);

  const token = useMemo(() => {
    return currentSn ? usersBySn[currentSn]?.token : null;
  }, [usersBySn, currentSn]);

  const initUserData = async () => {
    try {
      const response = await getDeviceInfoApi();
      // 查询设备信息，获取sn
      const sn = response.data.sn;
      setCurrentSn(sn);
      if (sn) {
        // 查询设备是否第一次登录，第一次登录，直接进登录页
        const response = await getUserInfoApi();
        if (response.code == 0) {
          const data = response.data;
          const firstLogin = data.firstLogin;
          updateFirstLogin(firstLogin);
          if (!firstLogin) {
            // 根据sn获取本地存储账号信息
            const userInfo = getUserInfoBySN(sn);
            if (userInfo?.userName && userInfo?.password) {
              // 根据本地账号信息自动登录获取token
              const response = await loginApi({
                userName: userInfo.userName,
                password: userInfo.password,
              });
              if (response.code == 0) {
                const token = response.data.token;
                updateToken(token);
              } else {
                clearCurrentUserInfo();
              }
            } else {
              clearCurrentUserInfo();
            }
          } else {
            clearCurrentUserInfo();
          }
        }
      }
    } catch (error) {
      // 不能清用户信息，很可能是服务没起来超时
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
      <div className="h-full">
        {token ? <RouterProvider router={router} /> : <Login />}
      </div>
    </ConfigProvider>
  );
};

export default App;
