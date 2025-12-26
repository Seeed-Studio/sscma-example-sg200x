import { Button, Form, Switch, Input, Modal } from "antd";
import { LoadingOutlined, InfoCircleOutlined, ReloadOutlined } from "@ant-design/icons";
import WarnImg from "@/assets/images/warn.png";
import LockImg from "@/assets/images/svg/lock.svg";
import ConnectedImg from "@/assets/images/svg/connected.svg";
import WireImg from "@/assets/images/svg/wire.svg";
import Wifi1 from "@/assets/images/svg/wifi_1.svg";
import Wifi2 from "@/assets/images/svg/wifi_2.svg";
import Wifi3 from "@/assets/images/svg/wifi_3.svg";
import Wifi4 from "@/assets/images/svg/wifi_4.svg";
import { useData, OperateType, FormType } from "./hook";

import {
  WifiAuth,
  NetworkStatus,
  WifiIpAssignmentRule,
  WifiEnable,
} from "@/enum/network";
import { requiredTrimValidate } from "@/utils/validate";

const wifiImg: {
  [prop: number]: string;
} = {
  1: Wifi1,
  2: Wifi2,
  3: Wifi3,
  4: Wifi4,
};

// Convert signal strength value to icon index
const getSignalIcon = (signal: number): number => {
  // Signal strength is negative, larger values (closer to 0) are stronger
  if (signal >= -50) return 4; // Very strong signal
  if (signal >= -60) return 3; // Strong signal
  if (signal >= -70) return 2; // Medium signal
  if (signal >= -80) return 1; // Weak signal
  return 1; // Very weak signal
};

const titleObj = {
  [FormType.Password]: "Password",
  [FormType.Disabled]: "Disable Wi-Fi",
};
function Network() {
  const {
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
    onRefreshNetworks,
  } = useData();

  return (
    <div className="px-16 pb-24">
      <div className="text-24 font-bold mt-24">Internet</div>
      {!state.wifiChecked &&
        state.etherStatus === NetworkStatus.Disconnected && (
          <div className="flex mt-10">
            <img className="w-24" src={WarnImg} alt="" />
            <span className="ml-12 self-center text-16">
              Not connected to a network
            </span>
          </div>
        )}
      {/* Ethernet */}
      {state.etherInfo && (
        <div className="mt-30">
          <div className="font-bold text-18 mb-20">Internet</div>
          <div className="border-b text-16">
            <div
              className="flex justify-between border-t py-10"
              onClick={onClickEthernetItem}
            >
              <span className="flex flex-1 truncate">
                {state.etherStatus === NetworkStatus.Connected && (
                  <img className="w-18 mr-12" src={ConnectedImg} alt="" />
                )}
                <img className="w-18 mr-12" src={WireImg} alt="" />
                <span className="self-center truncate">Ethernet</span>
              </span>
              <div className="flex items-center">
                <Button
                  type="text"
                  size="small"
                  icon={<InfoCircleOutlined />}
                  onClick={(event) => {
                    event.stopPropagation();
                    onClickEthernetItem();
                  }}
                />
              </div>
            </div>
          </div>
        </div>
      )}

      {/* WiFi switch: displayed when wifiEnable !== 2, state controlled by state.wifiChecked */}
      {state.wifiEnable !== WifiEnable.Disable && (
        <div className="mt-30">
          <div className="flex justify-between mb-20">
            <div className="font-bold text-18">Enable Wi-Fi</div>
            <Switch
              checked={state.wifiChecked}
              onChange={onSwitchEnabledWifi}
            />
          </div>
        </div>
      )}

      {/* My Networks - connected WiFi list (shown when Wi-Fi is enabled) */}
      {state.wifiChecked && state.connectedWifiInfoList.length > 0 && (
        <div className="mt-30">
          <div className="font-bold text-18 mb-20">My Networks</div>
          <div className="border-b text-16">
            {state.connectedWifiInfoList.map((wifiItem, index) => (
              <div
                className="flex justify-between border-t py-10"
                key={index}
                onClick={() => onClickWifiItem(wifiItem)}
              >
                <span className="flex flex-1 truncate">
                  <span className="self-center truncate flex items-center">
                    {wifiItem.status === NetworkStatus.Connecting ? (
                      <span className="mr-12 flex items-center">
                        <LoadingOutlined />
                      </span>
                    ) : wifiItem.status === NetworkStatus.Connected ? (
                      <img className="w-18 mr-12" src={ConnectedImg} alt="" />
                    ) : null}
                    {wifiItem.ssid}
                  </span>
                </span>
                <div className="flex items-center">
                  {wifiItem.auth == WifiAuth.Need && (
                    <div className="px-12">
                      <img
                        className="w-18"
                        src={LockImg}
                        alt=""
                        onClick={(event: React.MouseEvent) => {
                          event.stopPropagation();
                          onClickWifiItem(wifiItem);
                        }}
                      />
                    </div>
                  )}
                  <img
                    className="w-18"
                    src={wifiImg[getSignalIcon(wifiItem.signal)]}
                    alt=""
                  />
                  <Button
                    type="text"
                    size="small"
                    icon={<InfoCircleOutlined />}
                    onClick={(event) => {
                      event.stopPropagation();
                      onClickWifiInfo(wifiItem);
                    }}
                  />
                </div>
              </div>
            ))}
          </div>
        </div>
      )}

      {/* Other discovered networks (shown when Wi-Fi is enabled) */}
      {state.wifiChecked && state.wifiInfoList.length > 0 && (
        <div className="mt-30">
          <div className="flex justify-between items-center mb-20">
            <div className="font-bold text-18">Networks Found</div>
            <Button
              type="text"
              size="small"
              icon={
                state.refreshLoading ? <LoadingOutlined /> : <ReloadOutlined />
              }
              onClick={onRefreshNetworks}
              disabled={state.refreshLoading}
            />
          </div>
          <div className="border-b text-16">
            {state.wifiInfoList.map((wifiItem, index) => (
              <div
                className="flex justify-between border-t py-10"
                key={index}
                onClick={() => onClickWifiItem(wifiItem)}
              >
                <span className="flex flex-1 truncate">
                  <span className="self-center truncate flex items-center">
                    {wifiItem.status === NetworkStatus.Connecting ? (
                      <span className="mr-12 flex items-center">
                        <LoadingOutlined />
                      </span>
                    ) : wifiItem.status === NetworkStatus.Connected ? (
                      <img className="w-18 mr-12" src={ConnectedImg} alt="" />
                    ) : null}
                    {wifiItem.ssid}
                  </span>
                </span>
                <div className="flex items-center">
                  {wifiItem.auth == WifiAuth.Need && (
                    <div className="px-12">
                      <img
                        className="w-18"
                        src={LockImg}
                        alt=""
                        onClick={(event: React.MouseEvent) => {
                          event.stopPropagation();
                          onClickWifiItem(wifiItem);
                        }}
                      />
                    </div>
                  )}
                  <img
                    className="w-18"
                    src={wifiImg[getSignalIcon(wifiItem.signal)]}
                    alt=""
                  />
                  <Button
                    type="text"
                    size="small"
                    icon={<InfoCircleOutlined />}
                    onClick={(event) => {
                      event.stopPropagation();
                      onClickWifiInfo(wifiItem);
                    }}
                  />
                </div>
              </div>
            ))}
          </div>
        </div>
      )}
      <Modal
        open={state.visible}
        onCancel={toggleVisible}
        footer={null}
        centered
        width="90%"
        style={{ maxWidth: 480 }}
        title={titleObj[state.formType]}
        destroyOnClose
      >
        {state.formType === FormType.Disabled && (
          <div>
            <div className="text-3d text-16 mb-20">
              This action will prevent you from access this web dashboard. Are
              you sure want to turn off it now?
            </div>
            <Button
              block
              danger
              type="primary"
              loading={state.submitLoading}
              onClick={handleSwitchWifi}
            >
              Confirm
            </Button>
          </div>
        )}
        {state.formType === FormType.Password && (
          <Form
            ref={passwordFormRef}
            className="border-b-0"
            requiredMark={false}
            onFinish={onConnect}
            initialValues={{
              password: state.password,
            }}
          >
            <Form.Item
              name="password"
              label=""
              rules={[requiredTrimValidate()]}
            >
              <Input.Password placeholder="" allowClear maxLength={63} />
            </Form.Item>
            <Button
              block
              type="primary"
              htmlType="submit"
              loading={state.submitLoading}
            >
              Confirm
            </Button>
          </Form>
        )}
      </Modal>
      <Modal
        open={state.wifiVisible}
        onCancel={() => {
          setStates({
            wifiVisible: false,
          });
        }}
        footer={null}
        centered
        width="90%"
        style={{ maxWidth: 480 }}
        closable
        destroyOnClose
      >
        <div className="p-20 pr-6">
          <div className="text-3d pr-14 h-full overflow-y-auto flex-1 flex flex-col justify-between">
            <div className="flex justify-between">
              <div className="font-bold text-16 break-words">
                {state.selectedWifiInfo?.ssid}
              </div>
            </div>
            {state.selectedWifiInfo && state.selectedWifiInfo?.ssid && (
              <div className="flex mt-20">
                {state.selectedWifiInfo?.status === NetworkStatus.Connected ? (
                  <>
                    <Button
                      size="small"
                      color="danger"
                      variant="solid"
                      block
                      loading={
                        state.submitLoading &&
                        state.submitType == OperateType.Forget
                      }
                      onClick={() => onHandleOperate(OperateType.Forget)}
                    >
                      Forget
                    </Button>
                    <Button
                      size="small"
                      type="primary"
                      style={{ marginLeft: "12px" }}
                      block
                      loading={
                        state.submitLoading &&
                        state.submitType == OperateType.DisConnect
                      }
                      onClick={() => onHandleOperate(OperateType.DisConnect)}
                    >
                      Disconnect
                    </Button>
                  </>
                ) : (state.connectedWifiInfoList || []).some(
                    (item) => item.ssid === state.selectedWifiInfo?.ssid
                  ) ? (
                  <>
                    <Button
                      size="small"
                      color="danger"
                      variant="solid"
                      block
                      loading={
                        state.submitLoading &&
                        state.submitType == OperateType.Forget
                      }
                      onClick={() => onHandleOperate(OperateType.Forget)}
                    >
                      Forget
                    </Button>
                    <Button
                      size="small"
                      type="primary"
                      style={{ marginLeft: "12px" }}
                      block
                      loading={
                        state.submitLoading &&
                        state.submitType == OperateType.Connect
                      }
                      onClick={() => onHandleOperate(OperateType.Connect)}
                    >
                      Connect
                    </Button>
                  </>
                ) : (
                  <Button
                    size="small"
                    type="primary"
                    block
                    loading={
                      state.submitLoading &&
                      state.submitType == OperateType.Connect
                    }
                    onClick={() => onHandleOperate(OperateType.Connect)}
                  >
                    <span className="text-14">Connect</span>
                  </Button>
                )}
              </div>
            )}

            <div className="flex-1 mt-20 border-t">
              <div>
                <div className="flex justify-between border-b py-6">
                  <span className="self-center">MAC Address</span>
                  <span className="text-8d text-black opacity-60">
                    {state.selectedWifiInfo?.macAddress || "N/A"}
                  </span>
                </div>
              </div>

              {state.selectedWifiInfo && (
                <div>
                  <div className="font-bold border-b mt-24 py-6">
                    IPV4 ADDRESS
                  </div>
                  <div className="flex justify-between border-b py-6">
                    <span className="self-center">IP Address Assignment</span>
                    <span className="text-8d text-black opacity-60">
                      {state.selectedWifiInfo?.ipAssignment ===
                      WifiIpAssignmentRule.Automatic
                        ? "Automatic"
                        : "Static"}
                    </span>
                  </div>
                  <div className="flex justify-between border-b py-6">
                    <span className="self-center">IP Address</span>
                    <span className="text-8d text-black opacity-60">
                      {state.selectedWifiInfo?.ip || "N/A"}
                    </span>
                  </div>
                  <div className="flex justify-between border-b py-6">
                    <span className="self-center">Subnet Mask</span>
                    <span className="text-8d text-black opacity-60">
                      {state.selectedWifiInfo?.subnetMask || "N/A"}
                    </span>
                  </div>
                  <div className="flex justify-between border-b py-6">
                    <span className="self-center">DNS 1</span>
                    <span className="text-8d text-black opacity-60">
                      {state.selectedWifiInfo?.dns1 || "N/A"}
                    </span>
                  </div>
                  <div className="flex justify-between border-b py-6">
                    <span className="self-center">DNS 2</span>
                    <span className="text-8d text-black opacity-60">
                      {state.selectedWifiInfo?.dns2 || "N/A"}
                    </span>
                  </div>
                </div>
              )}
            </div>
          </div>
        </div>
      </Modal>
    </div>
  );
}

export default Network;
