import { message, Button } from "antd";
import { setDevicePowerApi } from "@/api/device/index";
import { PowerMode } from "@/enum";

function Power() {
  const onOperateDevice = async (mode: PowerMode) => {
    await setDevicePowerApi({ mode });
    message.success("Operate Success");
  };

  return (
    <div className="flex h-full justify-center p-32 flex-col text-18">
      <Button
        className="mb-12"
        color="danger"
        variant="outlined"
        onClick={() => onOperateDevice(PowerMode.Restart)}
      >
        Reboot
      </Button>
      <Button
        color="danger"
        variant="outlined"
        onClick={() => onOperateDevice(PowerMode.Shutdown)}
      >
        Shutdown
      </Button>
    </div>
  );
}

export default Power;
