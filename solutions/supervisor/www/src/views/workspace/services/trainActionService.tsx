import { Button, Modal, message } from "antd";
import { getModelApi, getModelV2Api } from "@/api/model";
import { getModelInfoApi, uploadModelApi } from "@/api/device";
import { DefaultFlowDataWithDashboard } from "@/utils";
import { waitForDashboardReady, waitForNodeRedReady } from "./flowService";

interface TrainReplayAction {
  action: "train";
  model_id: string;
  model_name?: string;
}

interface RunTrainActionOptions {
  modelId: string;
  modelName?: string;
  deviceIp?: string;
  setLoading: (v: boolean) => void;
  setLoadingTip: (tip: string) => void;
  messageApi: ReturnType<typeof message.useMessage>[0];
  modal: ReturnType<typeof Modal.useModal>[0];
  uploadAbortRef: { current: AbortController | null };
  createAppAndUpdateFlow: (params: {
    app_name?: string;
    flow_data?: string;
    model_data?: null;
    needUpdateFlow?: boolean;
    reloadOnFlowFail?: boolean;
  }) => Promise<boolean>;
  sendFlow: (flows?: string) => Promise<void>;
}

const reloadForTrainAction = (payload: TrainReplayAction) => {
  sessionStorage.setItem("sensecraft_action", JSON.stringify(payload));
  const url = new URL(window.location.href);
  url.search = "";
  url.searchParams.set("action", "train");
  url.searchParams.set("model_id", payload.model_id);
  if (payload.model_name) {
    url.searchParams.set("model_name", payload.model_name);
  }
  window.location.href = url.toString();
};

const confirmTrainReload = async (
  modal: ReturnType<typeof Modal.useModal>[0],
  reason: string,
  payload: TrainReplayAction
) => {
  const confirmed = await modal.confirm({
    title: "Train flow not fully ready",
    content: reason,
    okText: "Reload and retry train",
    cancelText: "Stay on current page",
  });
  if (confirmed) {
    reloadForTrainAction(payload);
  }
  return confirmed;
};

export const runTrainAction = async ({
  modelId,
  modelName,
  deviceIp,
  setLoading,
  setLoadingTip,
  messageApi,
  modal,
  uploadAbortRef,
  createAppAndUpdateFlow,
  sendFlow,
}: RunTrainActionOptions) => {
  if (!deviceIp) {
    messageApi.error("Device not connected");
    return;
  }

  try {
    console.info("train:start", { model_id: modelId, model_name: modelName });
    const replayPayload: TrainReplayAction = {
      action: "train",
      model_id: modelId,
      ...(modelName ? { model_name: modelName } : {}),
    };
    setLoading(true);
    let classesCsv: string | null = null;
    let classesList: string[] | null = null;
    let resolvedModelName = modelName;
    let trainDeviceType: number | null = null;
    let trainTaskId: number | null = null;
    setLoadingTip("Getting model info");
    console.info("train:step:get_model_info:start", { model_id: modelId });
    console.info("train:step:get_model_info:request", {
      url: `${import.meta.env.VITE_SENSECRAFT_TRAIN_API_URL}/v2/api/get_model`,
      params: { model_id: modelId },
    });
    const infoResp = await getModelV2Api(modelId);
    console.info("train:step:get_model_info:done", infoResp);
    trainDeviceType = infoResp.data?.device_type ?? null;
    trainTaskId = infoResp.data?.task_id ?? null;
    const classes = infoResp.data?.classes ?? infoResp.data?.metadata?.classes;
    if (Array.isArray(classes) && classes.length > 0) {
      classesList = classes;
      classesCsv = classes.join(",");
    }
    if (!resolvedModelName) {
      const displayName = infoResp.data?.display_name;
      const displayExt = infoResp.data?.display_ext;
      if (displayName && displayExt) {
        resolvedModelName = `${displayName}.${displayExt}`;
      } else if (displayName) {
        resolvedModelName = displayName;
      }
    }
    console.info("train:model_info_v2", {
      model_id: modelId,
      resolvedModelName,
      device_type: trainDeviceType,
      task_id: trainTaskId,
      classes: classesList,
    });

    if (trainDeviceType && trainDeviceType !== 40) {
      setLoading(false);
      setLoadingTip("");
      await modal.confirm({
        title: "Device type mismatch",
        content:
          "The selected model was trained for a different device type. Please choose a reCamera (device_type=40) model.",
        okText: "OK",
        cancelButtonProps: { style: { display: "none" } },
      });
      return;
    }

    setLoadingTip("Getting model");
    console.info("train:step:get_model:start", {
      model_id: modelId,
      task_id: trainTaskId,
      device_type: trainDeviceType,
    });
    console.info("train:step:get_model:request", {
      url: `${import.meta.env.VITE_SENSECRAFT_TRAIN_API_URL}/v1/api/get_model`,
      params: {
        model_id: modelId,
        task_id: trainTaskId ?? undefined,
        device_type: trainDeviceType ?? undefined,
      },
    });
    const blob = await getModelApi(modelId, {
      task_id: trainTaskId ?? undefined,
      device_type: trainDeviceType ?? undefined,
    });
    console.info("train:step:get_model:done", {
      size: blob.size,
      type: blob.type,
    });
    if (blob.type && blob.type.includes("text/html")) {
      try {
        const html = await blob.text();
        console.warn("train:get_model:html_response", html.slice(0, 500));
      } catch (error) {
        console.warn("train:get_model:html_parse_failed", error);
      }
    }

    setLoadingTip("Uploading model to device");
    const formData = new FormData();
    const fileName = resolvedModelName
      ? `${resolvedModelName}.cvimodel`
      : `${modelId}.cvimodel`;
    formData.append("model_file", blob, fileName);

    const modelInfo = {
      model_id: modelId,
      model_name: resolvedModelName || modelId,
      ...(classesList ? { classes: classesList } : {}),
    };
    formData.append("model_info", JSON.stringify(modelInfo));
    console.info("train:upload_model", {
      model_id: modelId,
      model_name: modelInfo.model_name,
      classes: classesList,
      model_info: modelInfo,
    });

    console.info("train:step:upload_model:start", { model_id: modelId });
    const uploadController = new AbortController();
    uploadAbortRef.current = uploadController;
    const uploadNoticeKey = "train-upload";
    messageApi.open({
      key: uploadNoticeKey,
      type: "warning",
      duration: 0,
      content:
        "Uploading model… You may cancel, but it can leave partial files on device.",
      btn: (
        <Button
          size="small"
          onClick={() => {
            modal.confirm({
              title: "Cancel upload?",
              content:
                "Canceling may leave partial model files on the device. You may need to re-upload or reboot.",
              okText: "Cancel Upload",
              cancelText: "Continue",
              onOk: () => {
                uploadAbortRef.current?.abort();
                messageApi.destroy(uploadNoticeKey);
                messageApi.warning("Upload canceled. Please re-upload if needed.");
              },
            });
          }}
        >
          Cancel Upload
        </Button>
      ),
    });
    const slowUploadTimer = window.setTimeout(() => {
      messageApi.warning(
        "Uploading model is taking longer than expected. Please check device connection."
      );
    }, 15000);
    const resp = await uploadModelApi(
      formData,
      (progress) => {
        setLoadingTip(`Uploading model to device: ${progress.toFixed(1)}%`);
      },
      uploadController.signal
    );
    window.clearTimeout(slowUploadTimer);
    messageApi.destroy(uploadNoticeKey);
    if (resp.code == 0) {
      messageApi.success("Model uploaded to device successfully");
      console.info("train:upload_model_success", { model_id: modelId });
      try {
        const deviceInfoResp = await getModelInfoApi();
        console.info("train:device_model_info", deviceInfoResp);
      } catch (error) {
        console.warn("train:get_device_model_info failed", error);
      }

      setLoadingTip("Waiting for Node-RED");
      const nodeRedReady = await waitForNodeRedReady();
      if (!nodeRedReady) {
        const shouldReload = await confirmTrainReload(
          modal,
          "Node-RED is still not running. Flow deployment may not take effect. Reload and retry train from the beginning?",
          replayPayload
        );
        if (!shouldReload) {
          messageApi.warning("Node-RED not ready yet");
        }
        return;
      }

      setLoadingTip("Creating app and updating flow");
      console.info("train:step:create_app:start");
      try {
        let flowData = DefaultFlowDataWithDashboard;
        console.info("train:flow:select_default", {
          flowLen: flowData?.length ?? 0,
        });
        if (classesCsv) {
          try {
            const flowObj = JSON.parse(flowData);
            if (Array.isArray(flowObj)) {
              const updated = flowObj.map((node: Record<string, unknown>) => {
                if (node.type === "model") {
                  return {
                    ...node,
                    classes: classesCsv,
                    uri: "",
                  };
                }
                return node;
              });
              flowData = JSON.stringify(updated);
            }
          } catch (error) {
            console.warn("train:update_flow_classes_failed", error);
          }
        }
        console.info("train:flow:final", {
          flowLen: flowData?.length ?? 0,
          hasUiTab: flowData?.includes('"ui-tab"') ?? false,
          hasUiTemplate: flowData?.includes('"ui-template"') ?? false,
        });

        const appNameBase = (resolvedModelName || modelId).replace(
          /\.cvimodel$/i,
          ""
        );
        const ok = await createAppAndUpdateFlow({
          app_name: `classify_${appNameBase}`,
          flow_data: flowData,
          model_data: null,
          needUpdateFlow: true,
          reloadOnFlowFail: false,
        });
        console.info("train:step:create_app:done", { ok });

        if (!ok) {
          console.warn("train:create_app_failed:fall_back_sendFlow");
          setLoadingTip("Deploying flow to device");
          try {
            await sendFlow(flowData);
          } catch (error) {
            console.error("train:fall_back_send_flow_failed", error);
            const shouldReload = await confirmTrainReload(
              modal,
              "Flow deployment failed because Node-RED is not ready. Reload and retry train from the beginning?",
              replayPayload
            );
            if (!shouldReload) {
              messageApi.warning("Flow deployment failed");
            }
            return;
          }
        }

        if (deviceIp) {
          setLoadingTip("Waiting for dashboard");
          console.info("train:step:wait_dashboard:start", {
            ip: deviceIp,
          });
          const ready = await waitForDashboardReady(deviceIp);
          console.info("train:step:wait_dashboard:done", { ready });
          if (ready) {
            sessionStorage.removeItem("sensecraft_action");
            const targetUrl = `http://${deviceIp}/#/dashboard`;
            console.info("train:redirect:dashboard", {
              from: window.location.href,
              to: targetUrl,
            });
            window.location.href = targetUrl;
          } else {
            const shouldReload = await confirmTrainReload(
              modal,
              "Dashboard did not become ready in time. Reload and retry train from the beginning?",
              replayPayload
            );
            if (!shouldReload) {
              messageApi.warning("Dashboard not ready yet");
            }
          }
        }
      } catch (error) {
        console.error("Create app/update flow error:", error);
      }
    } else {
      messageApi.error("Upload model to device failed");
    }
  } catch (error) {
    if (error instanceof DOMException && error.name === "AbortError") {
      console.warn("Train model upload aborted");
      sessionStorage.removeItem("sensecraft_action");
      setLoading(false);
      setLoadingTip("");
      return;
    }
    console.error("Train model error:", error);
    if (typeof error === "string") {
      console.error("train:error:string", error.slice(0, 500));
    } else if (error instanceof Error) {
      console.error("train:error:message", error.message);
    }
    const errorMsg =
      error && typeof error === "object" && "msg" in error
        ? (error as { msg?: string }).msg
        : "Train model failed";
    messageApi.error(errorMsg || "Train model failed");
  } finally {
    setLoading(false);
    setLoadingTip("");
  }
};
