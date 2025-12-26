import { useState, useEffect } from "react";
import {
  Modal,
  Button,
  Spin,
} from "antd";
import {
  WifiOutlined,
  UpOutlined,
  DownOutlined,
  SettingOutlined,
  FileOutlined,
} from "@ant-design/icons";
import { getDeviceListApi } from "@/api/device";
import { IIPDevice } from "@/api/device/device";
import recamera_logo from "@/assets/images/recamera_logo.png";
import useConfigStore from "@/store/config";
import Network from "@/views/network";
import styles from "./index.module.css";

const typePriority: Record<string, number> = {
  IPv: 1,
  wlan: 2,
  eth: 3,
  usb: 4,
};

const isValidType = (type: string): type is keyof typeof typePriority => {
  return type in typePriority;
};

const Workspace = () => {
  const { deviceInfo } = useConfigStore();

  const [isNetworkModalOpen, setIsNetworkModalOpen] = useState(false);
  const [devicelist, setDevicelist] = useState<IIPDevice[]>([]);
  const [showDeviceList, setShowDeviceList] = useState(false);
  const [deviceListRefreshing, setDeviceListRefreshing] = useState(false);

  const handleRefreshDeviceList = async () => {
    try {
      setDeviceListRefreshing(true);
      const { data } = await getDeviceListApi();
      const deviceList = data.deviceList.map((device) => {
        if (device.info && typeof device.info === "string") {
          const infoString = device.info.replace(/"/g, "");
          const infoParts = infoString.split(";");

          const infoObject = infoParts.reduce((acc, part) => {
            const [key, value] = part.split("=");
            (acc as Record<string, string>)[key] = value;
            return acc;
          }, {} as { sn: string; lastSix: string });

          if (infoObject.sn) {
            infoObject.lastSix = infoObject.sn.slice(-6);
          }

          device.info = infoObject;
        }
        return device;
      });

      const filteredDeviceList = deviceList.reduce((acc, device) => {
        const info = device.info as { sn: string; lastSix: string };
        if (info.sn === deviceInfo?.sn) {
          return acc;
        }
        const existingIndex = acc.findIndex(
          (d) => (d.info as { sn: string }).sn === info.sn
        );

        if (existingIndex === -1) {
          acc.push(device);
        } else {
          const existingDevice = acc[existingIndex];
          const currentType = device.type.replace(/[0-9]/g, "");
          const existingType = existingDevice.type.replace(/[0-9]/g, "");

          if (isValidType(currentType) && isValidType(existingType)) {
            if (typePriority[currentType] < typePriority[existingType]) {
              acc[existingIndex] = device;
            }
          }
        }
        return acc;
      }, [] as typeof deviceList);
      setDevicelist(filteredDeviceList);
      setDeviceListRefreshing(false);
      setShowDeviceList(filteredDeviceList.length > 0);
    } catch (error) {
      setDeviceListRefreshing(false);
      console.error("Error fetching DeviceList status:", error);
    }
  };

  useEffect(() => {
    if (deviceInfo?.sn) {
      handleRefreshDeviceList();
    }
  }, [deviceInfo?.sn]);

  const handleShowDeviceList = () => {
    if (!showDeviceList) {
      handleRefreshDeviceList();
    } else {
      setShowDeviceList(false);
    }
  };

  const handleNetworkModalOk = () => {
    setIsNetworkModalOpen(false);
  };

  const handleNetworkModalCancel = () => {
    setIsNetworkModalOpen(false);
  };

  const showNetworkModal = () => {
    setIsNetworkModalOpen(true);
  };

  const gotoConfig = () => {
    window.open(`http://${deviceInfo?.ip}/#/init`, "_blank");
  };

  const gotoOtherDevice = (ip: string) => {
    window.open(`http://${ip}/#/workspace`, "_blank");
  };

  const gotoWiki = () => {
    window.open(
      "https://wiki.seeedstudio.com/recamera_getting_started/",
      "_blank"
    );
  };

  return (
    <div className="flex h-full">
      <div className="flex flex-col justify-between w-320 bg-background relative p-24 pr-30 overflow-y-auto">
        {deviceInfo?.ip && (
          <div className="mb-10">
            <div className="text-20 font-semibold mb-12">My Device</div>
            <div className="max-h-400 overflow-y-auto">
              <div className="bg-selected rounded-4 p-8 border border-primary mb-12">
                <div className="flex items-center">
                  <img className="w-24 mr-8" src={recamera_logo} />
                  <div className="text-primary text-18 font-medium overflow-hidden text-ellipsis whitespace-nowrap">
                    {deviceInfo?.deviceName}
                  </div>
                </div>
                <div className="flex pt-6 text-text">
                  <span className="w-24 text-12">IP:</span>
                  <span className="text-12 ml-6">{deviceInfo?.ip}</span>
                </div>
                <div className="flex pt-5 text-text">
                  <span className="w-24 text-12">OS:</span>
                  <span className="text-12 ml-6">
                    {deviceInfo?.osVersion}
                  </span>
                </div>
                <div className="flex pt-5 text-text">
                  <span className="w-24 text-12">S/N:</span>
                  <span className="text-12 ml-6">{deviceInfo?.sn}</span>
                </div>
                <div className="flex mt-6">
                  <Button
                    className="mx-auto text-primary text-12 rounded-24 h-24"
                    icon={<SettingOutlined />}
                    onClick={gotoConfig}
                  >
                    Setting
                  </Button>
                  <Button
                    className="mx-auto text-primary text-12 rounded-24 h-24"
                    icon={<WifiOutlined />}
                    onClick={showNetworkModal}
                  >
                    Network
                  </Button>
                </div>
              </div>

              <Spin spinning={deviceListRefreshing}>
                <div className="rounded-4 p-8 border border-disable">
                  <div className="flex justify-between items-center text-12 text-primary">
                    <div>View device under same network</div>
                    <div
                      className="cursor-pointer"
                      onClick={handleShowDeviceList}
                    >
                      {showDeviceList ? <UpOutlined /> : <DownOutlined />}
                    </div>
                  </div>
                  {showDeviceList && devicelist?.length > 0 && (
                    <div className="max-h-200 overflow-y-auto mt-8">
                      {devicelist.map((device, index) => (
                        <div
                          key={index}
                          className="bg-background mb-8 p-4 rounded-4 hover:bg-selected cursor-pointer"
                          onClick={() => gotoOtherDevice(device.ip)}
                        >
                          <div className="text-14 text-3d overflow-hidden text-ellipsis whitespace-nowrap mb-4">
                            {device.device}
                          </div>
                          <div className="flex justify-between text-12 text-text">
                            <div>IP: {device.ip}</div>
                            <div>
                              S/N Last 6:{" "}
                              {typeof device.info == "object" &&
                                device.info.lastSix}
                            </div>
                          </div>
                        </div>
                      ))}
                    </div>
                  )}
                </div>
              </Spin>
              <div className="flex mt-10">
                <Button
                  className="mx-auto text-12 rounded-24 h-24"
                  type="primary"
                  disabled={deviceListRefreshing}
                  onClick={handleRefreshDeviceList}
                >
                  Refresh
                </Button>
              </div>
            </div>
          </div>
        )}
      </div>
      <div className="flex-1 flex justify-center items-center bg-gray-100">
        <div className="text-center">
          <h2 className="text-2xl font-semibold mb-4">reCamera Workspace</h2>
          <p className="text-gray-600 mb-6">
            {deviceInfo?.ip
              ? "Device connected. Use the settings to configure your device."
              : "Connect to a device to get started."}
          </p>
          <Button
            className="mr-4"
            icon={<FileOutlined />}
            onClick={gotoWiki}
          >
            reCamera Wiki
          </Button>
        </div>
      </div>
      <Modal
        title="Network"
        open={isNetworkModalOpen}
        destroyOnClose
        onCancel={handleNetworkModalCancel}
        footer={
          <Button type="primary" onClick={handleNetworkModalOk}>
            OK
          </Button>
        }
      >
        <div className={styles.networkModal}>
          <Network />
        </div>
      </Modal>
    </div>
  );
};

export default Workspace;
