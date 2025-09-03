import { useEffect } from "react";
import { CheckCircleFilled } from "@ant-design/icons";
import useConfigStore from "@/store/config";
import { getWiFiInfoListApi } from "@/api/network";
import { NetworkStatus } from "@/enum/network";
import { queryDeviceInfoApi } from "@/api/device/index";

const infoList = [
  {
    label: "IP",
    key: "ip",
  },
  {
    label: "Wi-Fi IP",
    key: "wifiIp",
  },
  {
    label: "Mask",
    key: "mask",
  },
  {
    label: "Gateway",
    key: "gateway",
  },
  {
    label: "DNS",
    key: "dns",
  },
];
function Init() {
  const { deviceInfo, updateWifiStatus, updateEtherStatus, updateDeviceInfo } =
    useConfigStore();

  const getWifiStatus = async () => {
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

    const res = await queryDeviceInfoApi();
    updateDeviceInfo(res.data);
  };
  useEffect(() => {
    getWifiStatus();
  }, []);

  return (
    <div className="p-16">
      <div className="rounded-16 bg-white p-30">
        <div className="">
          {infoList.map((item, index) => {
            return (
              <div className="flex justify-between mb-8" key={index}>
                <span className="text-black opacity-60 text-right w-2/5">
                  {item.label}：
                </span>
                <span className="font-bold flex-1  ">
                  {deviceInfo[item.key]}
                </span>
              </div>
            );
          })}
        </div>
      </div>

      {deviceInfo.wifiIp && deviceInfo.wifiIp != "-" && (
        <div>
          <div className="text-center flex justify-center py-40">
            <CheckCircleFilled style={{ fontSize: 48, color: "#8fc31f" }} />
          </div>
          {deviceInfo.wifiIp != deviceInfo.ip && (
            <div className="text-center text-18 px-32  break-all">
              <span>
                Connect to '{deviceInfo.deviceName}' then you can explore the
                dashboard by visit
              </span>
              <a
                className="text-primary ml-2"
                href={"http://" + deviceInfo.wifiIp + "/"}
              >
                {deviceInfo.wifiIp}
              </a>
            </div>
          )}
        </div>
      )}
    </div>
  );
}

export default Init;
