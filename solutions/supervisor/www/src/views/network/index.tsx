import {
  Button,
  Form,
  Switch,
  Input,
  Modal,
  Tabs,
  Select,
} from "antd";
import { LoadingOutlined, InfoCircleOutlined, PlusOutlined } from "@ant-design/icons";
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
  AntennaEnable,
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
  [FormType.HalowConfig]: "HaLow Configuration",
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
    onSwitchEnabledHalow,
    handleSwitchHalow,
    handleSwitchAntenna,
    onClickHalowItem,
    onClickHalowInfo,
    onHandleHalowOperate,
    onConnectHalow,
    onOpenManualHalowConfig,
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

      {/* WiFi和Halow Tab切换 */}
      <div className="mt-30">
        <Tabs
          activeKey={state.activeTab}
          size="large"
          onChange={(key) => setStates({ activeTab: key as "wifi" | "halow" })}
          items={[
            {
              key: "wifi",
              label: "Wi-Fi",
              children: (
                <>
                  {/* WiFi开关：当 wifiEnable !== 2 时显示，状态由 state.wifiChecked 控制 */}
                  {state.wifiEnable !== WifiEnable.Disable && (
                    <div className="mt-20">
                      <div className="flex justify-between mb-20">
                        <div className="font-bold text-18">Enable Wi-Fi</div>
                        <Switch
                          checked={state.wifiChecked}
                          onChange={onSwitchEnabledWifi}
                        />
                      </div>
                    </div>
                  )}

                  {/* 我的网络 - 已连接过的WiFi列表（Wi-Fi 开启时显示） */}
                  {state.wifiChecked &&
                    state.connectedWifiInfoList.length > 0 && (
                      <div className="mt-30">
                        <div className="font-bold text-18 mb-20">
                          My Networks
                        </div>
                        <div className="border-b text-16">
                          {state.connectedWifiInfoList.map(
                            (wifiItem, index) => (
                              <div
                                className="flex justify-between border-t py-10"
                                key={index}
                                onClick={() => onClickWifiItem(wifiItem)}
                              >
                                <span className="flex flex-1 truncate">
                                  <span className="self-center truncate flex items-center">
                                    {wifiItem.status ===
                                    NetworkStatus.Connecting ? (
                                      <span className="mr-12 flex items-center">
                                        <LoadingOutlined />
                                      </span>
                                    ) : wifiItem.status ===
                                      NetworkStatus.Connected ? (
                                      <img
                                        className="w-18 mr-12"
                                        src={ConnectedImg}
                                        alt=""
                                      />
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
                                    src={
                                      wifiImg[getSignalIcon(wifiItem.signal)]
                                    }
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
                            )
                          )}
                        </div>
                      </div>
                    )}

                  {/* 其他发现的网络（Wi-Fi 开启时显示） */}
                  {state.wifiChecked && state.wifiInfoList.length > 0 && (
                    <div className="mt-30">
                      <div className="font-bold text-18 mb-20">
                        Networks Found
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
                                {wifiItem.status ===
                                NetworkStatus.Connecting ? (
                                  <span className="mr-12 flex items-center">
                                    <LoadingOutlined />
                                  </span>
                                ) : wifiItem.status ===
                                  NetworkStatus.Connected ? (
                                  <img
                                    className="w-18 mr-12"
                                    src={ConnectedImg}
                                    alt=""
                                  />
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
                </>
              ),
            },
            {
              key: "halow",
              label: "HaLow",
              children: (
                <>
                  {/* Halow开关：当 halowEnable !== 2 时显示，状态由 state.halowChecked 控制 */}
                  {state.halowEnable !== 2 && (
                    <div className="mt-20">
                      <div className="flex justify-between mb-20">
                        <div className="font-bold text-18">Enable HaLow</div>
                        <Switch
                          checked={state.halowChecked}
                          onChange={onSwitchEnabledHalow}
                        />
                      </div>
                    </div>
                  )}

                  {/* Antenna开关：显示天线状态 */}
                  {state.halowEnable !== 2 && state.halowChecked && (
                    <div className="mt-20">
                      <div className="flex justify-between items-center mb-20">
                        <div className="font-bold text-18">IPEX Antenna</div>
                        <div className="flex items-center">
                          <Switch
                            checked={state.antennaEnable === AntennaEnable.RF2}
                            onChange={handleSwitchAntenna}
                          />
                        </div>
                      </div>
                    </div>
                  )}


                  {/* 我的网络 - 已连接过的Halow列表（HaLow 开启时显示） */}
                  {state.halowChecked &&
                    state.connectedHalowInfoList.length > 0 && (
                      <div className="mt-30">
                        <div className="font-bold text-18 mb-20">
                          My Networks
                        </div>
                        <div className="border-b text-16">
                          {state.connectedHalowInfoList.map(
                            (halowItem, index) => (
                              <div
                                className="flex justify-between border-t py-10"
                                key={index}
                                onClick={() => onClickHalowItem(halowItem)}
                              >
                                <span className="flex flex-1 truncate">
                                  <span className="self-center truncate flex items-center">
                                    {halowItem.status ===
                                    NetworkStatus.Connecting ? (
                                      <span className="mr-12 flex items-center">
                                        <LoadingOutlined />
                                      </span>
                                    ) : halowItem.status ===
                                      NetworkStatus.Connected ? (
                                      <img
                                        className="w-18 mr-12"
                                        src={ConnectedImg}
                                        alt=""
                                      />
                                    ) : null}
                                    {halowItem.ssid}
                                  </span>
                                </span>
                                <div className="flex items-center">
                                  {halowItem.auth == WifiAuth.Need && (
                                    <div className="px-12">
                                      <img
                                        className="w-18"
                                        src={LockImg}
                                        alt=""
                                        onClick={(event: React.MouseEvent) => {
                                          event.stopPropagation();
                                          onClickHalowItem(halowItem);
                                        }}
                                      />
                                    </div>
                                  )}
                                  <img
                                    className="w-18"
                                    src={
                                      wifiImg[getSignalIcon(halowItem.signal)]
                                    }
                                    alt=""
                                  />
                                  <Button
                                    type="text"
                                    size="small"
                                    icon={<InfoCircleOutlined />}
                                    onClick={(event) => {
                                      event.stopPropagation();
                                      onClickHalowInfo(halowItem);
                                    }}
                                  />
                                </div>
                              </div>
                            )
                          )}
                        </div>
                      </div>
                    )}

                  {/* 其他发现的网络（HaLow 开启时显示） */}
                  {state.halowChecked && (
                    <div className="mt-30">
                      <div className="flex justify-between items-center mb-20">
                        <div className="font-bold text-18">Networks Found</div>
                        <Button
                          type="primary"
                          icon={<PlusOutlined />}
                          size="small"
                          onClick={onOpenManualHalowConfig}
                        />
                      </div>
                      {state.halowInfoList.length > 0 && (
                        <div className="border-b text-16">
                          {state.halowInfoList.map((halowItem, index) => (
                            <div
                              className="flex justify-between border-t py-10"
                              key={index}
                              onClick={() => onClickHalowItem(halowItem)}
                            >
                              <span className="flex flex-1 truncate">
                                <span className="self-center truncate flex items-center">
                                  {halowItem.status ===
                                  NetworkStatus.Connecting ? (
                                    <span className="mr-12 flex items-center">
                                      <LoadingOutlined />
                                    </span>
                                  ) : halowItem.status ===
                                    NetworkStatus.Connected ? (
                                    <img
                                      className="w-18 mr-12"
                                      src={ConnectedImg}
                                      alt=""
                                    />
                                  ) : null}
                                  {halowItem.ssid}
                                </span>
                              </span>
                              <div className="flex items-center">
                                {halowItem.auth == WifiAuth.Need && (
                                  <div className="px-12">
                                    <img
                                      className="w-18"
                                      src={LockImg}
                                      alt=""
                                      onClick={(event: React.MouseEvent) => {
                                        event.stopPropagation();
                                        onClickHalowItem(halowItem);
                                      }}
                                    />
                                  </div>
                                )}
                                <img
                                  className="w-18"
                                  src={wifiImg[getSignalIcon(halowItem.signal)]}
                                  alt=""
                                />
                                <Button
                                  type="text"
                                  size="small"
                                  icon={<InfoCircleOutlined />}
                                  onClick={(event) => {
                                    event.stopPropagation();
                                    onClickHalowInfo(halowItem);
                                  }}
                                />
                              </div>
                            </div>
                          ))}
                        </div>
                      )}
                    </div>
                  )}
                </>
              ),
            },
          ]}
        />
      </div>
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
              onClick={
                state.activeTab === "wifi"
                  ? handleSwitchWifi
                  : handleSwitchHalow
              }
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
            onFinish={state.activeTab === "wifi" ? onConnect : onConnectHalow}
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
        {state.formType === FormType.HalowConfig && (
          <Form
            ref={passwordFormRef}
            className="border-b-0"
            requiredMark={false}
            onFinish={(values) => {
              const halowConfig = {
                country: values.country || "US",
                mode: values.mode,
                encryption: values.encryption,
              };
              // 如果有 selectedHalowInfo，使用它的 ssid；否则使用表单输入的 ssid
              const ssid = state.selectedHalowInfo?.ssid || values.ssid;
              onConnectHalow({ ...values, halowConfig }, ssid);
            }}
            initialValues={{
              country: "US",
              mode: 0,
              encryption: "WPA3-SAE",
            }}
          >
            {/* 如果没有 selectedHalowInfo（手动添加），显示 SSID 输入框 */}
            {!state.selectedHalowInfo && (
              <Form.Item
                name="ssid"
                label="SSID"
                rules={[requiredTrimValidate()]}
              >
                <Input placeholder="Enter SSID" allowClear maxLength={63} />
              </Form.Item>
            )}
            <Form.Item
              name="country"
              label="Country"
              rules={[{ required: true, message: "Please select country" }]}
            >
              <Select
                options={[
                  { label: "US", value: "US" },
                  { label: "AU", value: "AU" },
                ]}
              />
            </Form.Item>
            <Form.Item
              name="mode"
              label="Mode"
              rules={[{ required: true, message: "Please select mode" }]}
            >
              <Select
                options={[
                  { label: "no WDS", value: 0 },
                  { label: "WDS", value: 1 },
                ]}
              />
            </Form.Item>
            <Form.Item
              name="encryption"
              label="Encryption"
              rules={[{ required: true, message: "Please select encryption" }]}
            >
              <Select
                options={[
                  { label: "WPA3-SAE", value: "WPA3-SAE" },
                  { label: "OWE", value: "OWE" },
                  { label: "No encryption", value: "No encryption" },
                ]}
              />
            </Form.Item>
            {/* 密码输入框显示逻辑 */}
            <Form.Item noStyle shouldUpdate={(prevValues, currentValues) => prevValues.encryption !== currentValues.encryption}>
              {({ getFieldValue }) => {
                const encryption = getFieldValue("encryption");
                // 如果是点击列表项，根据 auth 判断；如果是手动添加，根据加密方式判断
                if (state.selectedHalowInfo) {
                  // 点击列表项：根据 auth 判断
                  return state.selectedHalowInfo.auth === WifiAuth.Need ? (
                    <Form.Item
                      name="password"
                      label="Password"
                      rules={[requiredTrimValidate()]}
                    >
                      <Input.Password placeholder="" allowClear maxLength={63} />
                    </Form.Item>
                  ) : null;
                } else {
                  // 手动添加：根据加密方式判断，OWE 和 No encryption 不需要密码
                  return encryption && encryption !== "OWE" && encryption !== "No encryption" ? (
                    <Form.Item
                      name="password"
                      label="Password"
                      rules={[requiredTrimValidate()]}
                    >
                      <Input.Password placeholder="" allowClear maxLength={63} />
                    </Form.Item>
                  ) : null;
                }
              }}
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
                {state.activeTab === "wifi"
                  ? state.selectedWifiInfo?.ssid
                  : state.selectedHalowInfo?.ssid}
              </div>
            </div>
            {state.activeTab === "wifi" &&
              state.selectedWifiInfo &&
              state.selectedWifiInfo?.ssid && (
                <div className="flex mt-20">
                  {state.selectedWifiInfo?.status ===
                  NetworkStatus.Connected ? (
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
            {state.activeTab === "halow" &&
              state.selectedHalowInfo &&
              state.selectedHalowInfo?.ssid && (
                <div className="flex mt-20">
                  {state.selectedHalowInfo?.status ===
                  NetworkStatus.Connected ? (
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
                        onClick={() => onHandleHalowOperate(OperateType.Forget)}
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
                        onClick={() =>
                          onHandleHalowOperate(OperateType.DisConnect)
                        }
                      >
                        Disconnect
                      </Button>
                    </>
                  ) : (state.connectedHalowInfoList || []).some(
                      (item) => item.ssid === state.selectedHalowInfo?.ssid
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
                        onClick={() => onHandleHalowOperate(OperateType.Forget)}
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
                        onClick={() =>
                          onHandleHalowOperate(OperateType.Connect)
                        }
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
                      onClick={() => onHandleHalowOperate(OperateType.Connect)}
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
                    {state.activeTab === "wifi"
                      ? state.selectedWifiInfo?.macAddress || "N/A"
                      : state.selectedHalowInfo?.macAddress || "N/A"}
                  </span>
                </div>
              </div>

              {(state.activeTab === "wifi"
                ? state.selectedWifiInfo
                : state.selectedHalowInfo) && (
                <div>
                  <div className="font-bold border-b mt-24 py-6">
                    IPV4 ADDRESS
                  </div>
                  <div className="flex justify-between border-b py-6">
                    <span className="self-center">IP Address Assignment</span>
                    <span className="text-8d text-black opacity-60">
                      {(state.activeTab === "wifi"
                        ? state.selectedWifiInfo
                        : state.selectedHalowInfo
                      )?.ipAssignment === WifiIpAssignmentRule.Automatic
                        ? "Automatic"
                        : "Static"}
                    </span>
                  </div>
                  <div className="flex justify-between border-b py-6">
                    <span className="self-center">IP Address</span>
                    <span className="text-8d text-black opacity-60">
                      {(state.activeTab === "wifi"
                        ? state.selectedWifiInfo
                        : state.selectedHalowInfo
                      )?.ip || "N/A"}
                    </span>
                  </div>
                  <div className="flex justify-between border-b py-6">
                    <span className="self-center">Subnet Mask</span>
                    <span className="text-8d text-black opacity-60">
                      {(state.activeTab === "wifi"
                        ? state.selectedWifiInfo
                        : state.selectedHalowInfo
                      )?.subnetMask || "N/A"}
                    </span>
                  </div>
                  <div className="flex justify-between border-b py-6">
                    <span className="self-center">DNS 1</span>
                    <span className="text-8d text-black opacity-60">
                      {(state.activeTab === "wifi"
                        ? state.selectedWifiInfo
                        : state.selectedHalowInfo
                      )?.dns1 || "N/A"}
                    </span>
                  </div>
                  <div className="flex justify-between border-b py-6">
                    <span className="self-center">DNS 2</span>
                    <span className="text-8d text-black opacity-60">
                      {(state.activeTab === "wifi"
                        ? state.selectedWifiInfo
                        : state.selectedHalowInfo
                      )?.dns2 || "N/A"}
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
