import { useEffect, useState, useMemo } from "react";
import { createHashRouter, RouterProvider } from "react-router-dom";
import Routes from "@/router";
import Login from "@/views/login";
import Loading from "@/components/loading";
import { ConfigProvider, Modal, Button } from "antd";
import { IsUpgrading, UpdateStatus, DeviceChannleMode } from "@/enum";
import {
  queryDeviceInfoApi,
  getSystemUpdateVesionInfoApi,
} from "@/api/device/index";
import useUserStore from "@/store/user";
import useConfigStore from "@/store/config";
import { getUserInfoApi } from "@/api/user";
import { ServiceStatus } from "@/enum";

const router = createHashRouter(Routes);

const App = () => {
  const {
    currentSn,
    usersBySn,
    setCurrentSn,
    updateFirstLogin,
    clearCurrentUserInfo,
  } = useUserStore();

  const { setSystemUpdateState } = useConfigStore();

  const [serviceStatus, setServiceStatus] = useState<ServiceStatus>(
    ServiceStatus.STARTING
  );
  const [isNewVersionModalOpen, setIsNewVersionModalOpen] = useState(false);
  const [newVersion, setNewVersion] = useState("");

  useEffect(() => {
    initUserData();
  }, []);

  const token = useMemo(() => {
    return currentSn ? usersBySn[currentSn]?.token : null;
  }, [usersBySn, currentSn]);

  const initUserData = async () => {
    try {
      const response = await queryDeviceInfoApi();
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
          // if (!firstLogin) {
          //   // 根据sn获取本地存储账号信息
          //   const userInfo = getUserInfoBySN(sn);
          //   if (userInfo?.userName && userInfo?.password) {
          //     // 根据本地账号信息自动登录获取token
          //     const response = await loginApi({
          //       userName: userInfo.userName,
          //       password: userInfo.password,
          //     });
          //     if (response.code == 0) {
          //       const token = response.data.token;
          //       updateToken(token);
          //     } else {
          //       clearCurrentUserInfo();
          //     }
          //   } else {
          //     clearCurrentUserInfo();
          //   }
          // } else {
          //   clearCurrentUserInfo();
          // }
          if (firstLogin) {
            clearCurrentUserInfo();
          }
        }
      }
    } catch (error) {
      // 不能清用户信息，很可能是服务没起来超时
    }
  };

  const checkNewVersion = async () => {
    try {
      const response = await queryDeviceInfoApi();
      const deviceInfo = response.data;
      const url =
        deviceInfo.channel == DeviceChannleMode.Official
          ? deviceInfo.officialUrl
          : deviceInfo.serverUrl;

      const { code, data } = await getSystemUpdateVesionInfoApi({
        channel: deviceInfo.channel,
        url: url,
      });

      if (code == 0) {
        if (data.isUpgrading === IsUpgrading.Yes) {
          return;
        }
        if (!data.osName || !data.osVersion) {
          return;
        }
        if (
          deviceInfo.osName != data.osName ||
          deviceInfo.osVersion != data.osVersion
        ) {
          setNewVersion(data.osVersion);
          setSystemUpdateState({
            status: UpdateStatus.NeedUpdate,
          });
          setIsNewVersionModalOpen(true);
        }
      }
    } catch (error) {}
  };

  useEffect(() => {
    if (token && serviceStatus === ServiceStatus.RUNNING) {
      checkNewVersion();
    }
  }, [token, serviceStatus]);

  const handleCancel = () => {
    setIsNewVersionModalOpen(false);
  };

  const handleOk = () => {
    setIsNewVersionModalOpen(false);
    window.location.href = `${window.location.origin}/#/system`;
  };

  const handleServiceStatusChange = (serviceStatus: ServiceStatus) => {
    setServiceStatus(serviceStatus);
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
        {token ? (
          <div className="h-full">
            {serviceStatus === ServiceStatus.RUNNING ? (
              <RouterProvider router={router} />
            ) : (
              <Loading onServiceStatusChange={handleServiceStatusChange} />
            )}
          </div>
        ) : (
          <Login />
        )}
        <Modal
          title="New Version Release"
          open={isNewVersionModalOpen}
          onCancel={handleCancel}
          footer={
            <Button type="primary" onClick={handleOk}>
              Go to update
            </Button>
          }
        >
          <p style={{ marginBottom: "6px" }}>
            reCamera is now on version {newVersion} now. Check update details
            here:
          </p>
          <a
            href="https://github.com/Seeed-Studio/reCamera-OS/releases"
            style={{
              color: "#4096ff",
            }}
            target="_blank"
          >
            https://github.com/Seeed-Studio/reCamera-OS/releases
          </a>
        </Modal>
      </div>
    </ConfigProvider>
  );
};

export default App;
