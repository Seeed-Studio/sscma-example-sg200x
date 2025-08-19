import { Form, Input, Button, Mask } from "antd-mobile";
import { Button as AntdButton, Switch } from "antd";
import { CloseOutlined, LoadingOutlined } from "@ant-design/icons";
import CommonPopup from "@/components/common-popup";
import WarnImg from "@/assets/images/warn.png";
import LockImg from "@/assets/images/svg/lock.svg";
import ConnectedImg from "@/assets/images/svg/connected.svg";
import WireImg from "@/assets/images/svg/wire.svg";
import Wifi1 from "@/assets/images/svg/wifi_1.svg";
import Wifi2 from "@/assets/images/svg/wifi_2.svg";
import Wifi3 from "@/assets/images/svg/wifi_3.svg";
import Wifi4 from "@/assets/images/svg/wifi_4.svg";
import { EyeInvisibleOutline, EyeOutline } from "antd-mobile-icons";
import { useData, OperateType, FormType } from "./hook";

import {
  WifiAuth,
  WifiConnectedStatus,
  NetworkStatus,
  WifiIpAssignmentRule,
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

// 将信号强度值转换为图标索引
const getSignalIcon = (signal: number): number => {
  // 信号强度是负值，数值越大（越接近0）信号越强
  if (signal >= -50) return 4; // 信号很强
  if (signal >= -60) return 3; // 信号强
  if (signal >= -70) return 2; // 信号中等
  if (signal >= -80) return 1; // 信号弱
  return 1; // 信号很弱
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
    onClickEthernetItem,
    handleSwitchWifi,
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
      {/* 有线网络 */}
      {state.etherInfo && (
        <div className="mt-30">
          <div className="font-bold text-18 mb-20">Internet</div>
          <div className="border-b text-16">
            <div
              className="flex justify-between border-t py-10"
              onClick={onClickEthernetItem}
            >
              <span className="flex flex-1 truncate">
                <img className="w-18 mr-12" src={WireImg} alt="" />
                <span className="self-center truncate">Ethernet</span>
              </span>
            </div>
          </div>
        </div>
      )}

      {/* WiFi开关 */}
      {state.wifiChecked && (
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

      {/* 我的网络 - 已连接过的WiFi列表 */}
      {state.connectedWifiInfoList.length > 0 && (
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
                <div className="flex">
                  {wifiItem.auth == WifiAuth.Need && (
                    <div className="px-12">
                      <img
                        className="w-18"
                        src={LockImg}
                        alt=""
                        onClick={(event: React.MouseEvent) => {
                          event.stopPropagation();
                          onClickWifiItem(wifiItem, true);
                        }}
                      />
                    </div>
                  )}
                  <img
                    className="w-18"
                    src={wifiImg[getSignalIcon(wifiItem.signal)]}
                    alt=""
                  />
                </div>
              </div>
            ))}
          </div>
        </div>
      )}

      {/* 其他发现的网络 */}
      {state.wifiInfoList.length > 0 && (
        <div className="mt-30">
          <div className="font-bold text-18 mb-20">Networks Found</div>
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
                <div className="flex">
                  {wifiItem.auth == WifiAuth.Need && (
                    <div className="px-12">
                      <img
                        className="w-18"
                        src={LockImg}
                        alt=""
                        onClick={(event: React.MouseEvent) => {
                          event.stopPropagation();
                          onClickWifiItem(wifiItem, true);
                        }}
                      />
                    </div>
                  )}
                  <img
                    className="w-18"
                    src={wifiImg[getSignalIcon(wifiItem.signal)]}
                    alt=""
                  />
                </div>
              </div>
            ))}
          </div>
        </div>
      )}
      <CommonPopup
        visible={state.visible}
        title={titleObj[state.formType]}
        onCancel={toggleVisible}
      >
        {state.formType === FormType.Disabled && (
          <div>
            <div className="text-3d text-16 mb-20">
              This action will prevent you from access this web dashboard. Are
              you sure want to turn off it now?
            </div>
            <Button
              block
              shape="rounded"
              type="submit"
              color="danger"
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
            requiredMarkStyle="none"
            onFinish={onConnect}
            initialValues={{
              password: state.password,
            }}
            footer={
              <Button
                block
                type="submit"
                color="primary"
                loading={state.submitLoading}
              >
                Confirm
              </Button>
            }
          >
            <Form.Item
              name="password"
              label=""
              rules={[requiredTrimValidate()]}
              extra={
                <div>
                  {!state.passwordVisible ? (
                    <EyeInvisibleOutline
                      fontSize={18}
                      onClick={() =>
                        setStates({
                          passwordVisible: true,
                        })
                      }
                    />
                  ) : (
                    <EyeOutline
                      fontSize={18}
                      onClick={() =>
                        setStates({
                          passwordVisible: false,
                        })
                      }
                    />
                  )}
                </div>
              }
            >
              <Input
                type={state.passwordVisible ? "text" : "password"}
                className="border rounded-6 p-10 pr-6"
                placeholder=""
                clearable
                maxLength={63}
              />
            </Form.Item>
          </Form>
        )}
      </CommonPopup>
      <Mask
        visible={state.wifiVisible}
        onMaskClick={() => {
          setStates({
            wifiVisible: false,
          });
        }}
      >
        <div
          className="absolute"
          style={{
            height: "60%",
            width: "90%",
            left: "50%",
            top: "50%",
            transform: "translate(-50%,-50%)",
            maxWidth: "480px",
          }}
        >
          <div className="h-full p-20 pr-6 bg-white rounded-16">
            <div className="text-3d  pr-14 h-full overflow-y-auto flex-1 flex flex-col justify-between">
              <div className="flex justify-between">
                <div className="font-bold text-16 break-words">
                  {state.selectedWifiInfo?.ssid}
                </div>
                <AntdButton
                  icon={<CloseOutlined />}
                  className="border-none"
                  onClick={() => {
                    setStates({
                      wifiVisible: false,
                    });
                  }}
                />
              </div>
              {state.selectedWifiInfo &&
                state.selectedWifiInfo !== state.etherInfo && (
                  <div className="flex mt-20">
                    {state.selectedWifiInfo?.connectedStatus ==
                    WifiConnectedStatus.No ? (
                      <Button
                        fill="none"
                        size="small"
                        className="flex-1 bg-primary text-white"
                        loading={state.submitLoading}
                        onClick={() => onHandleOperate(OperateType.Connect)}
                      >
                        <span className="text-14">Connect</span>
                      </Button>
                    ) : (
                      <>
                        <Button
                          fill="none"
                          size="small"
                          className="flex-1 mr-28 bg-error text-white"
                          loading={
                            state.submitLoading &&
                            state.submitType == OperateType.Forget
                          }
                          onClick={() => onHandleOperate(OperateType.Forget)}
                        >
                          <span className="text-14">Forget</span>
                        </Button>
                        <Button
                          fill="none"
                          size="small"
                          className="flex-1 bg-primary text-white"
                          loading={
                            state.submitLoading &&
                            state.submitType == OperateType.DisConnect
                          }
                          onClick={() =>
                            onHandleOperate(OperateType.DisConnect)
                          }
                        >
                          <span className="text-14">Disconnect</span>
                        </Button>
                      </>
                    )}
                  </div>
                )}

              <div className="flex-1 mt-20 border-t">
                {/* 基本信息 */}
                <div>
                  <div className="flex justify-between border-b py-6">
                    <span className="self-center">MAC Address</span>
                    <span className="text-8d text-black opacity-60">
                      {state.selectedWifiInfo?.macAddress || "N/A"}
                    </span>
                  </div>
                </div>

                {/* IP地址信息 - 显示IP相关信息 */}
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
        </div>
      </Mask>
    </div>
  );
}

export default Network;
