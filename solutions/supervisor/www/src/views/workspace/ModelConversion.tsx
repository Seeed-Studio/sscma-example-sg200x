import { useState, useEffect, useRef } from "react";
import {
  Modal,
  Button,
  Upload,
  message,
  ButtonProps,
  Spin,
  Tooltip,
} from "antd";
import {
  UploadOutlined,
  DeleteOutlined,
  CheckCircleOutlined,
  CloseCircleOutlined,
  LoadingOutlined,
} from "@ant-design/icons";
import {
  createTaskApi,
  getTrainingRecordsApi,
  getTrainStatusApi,
  getModelApi,
  deleteModelApi,
  stopTaskApi,
} from "@/api/model";
import type { IModelRecord } from "@/api/model/model.d";
import { uploadModelApi } from "@/api/device";
import {
  getFlows,
  saveFlows,
  getFlowsState,
  setFlowsState,
} from "@/api/nodered";
import { ModelConversionStatus } from "@/enum";
import usePlatformStore from "@/store/platform";
import useConfigStore from "@/store/config";
import styles from "./index.module.css";

interface ModelConversionProps {
  setLoading: (loading: boolean) => void;
  setLoadingTip: (tip: string) => void;
  messageApi: ReturnType<typeof message.useMessage>[0];
  modal: ReturnType<typeof Modal.useModal>[0];
}

const ModelConversion = ({
  setLoading,
  setLoadingTip,
  messageApi,
  modal,
}: ModelConversionProps) => {
  const { token } = usePlatformStore();
  const { deviceInfo } = useConfigStore();
  const [modelList, setModelList] = useState<IModelRecord[]>([]);
  const [modelListLoading, setModelListLoading] = useState(false);
  const [stoppingTaskIds, setStoppingTaskIds] = useState<Set<string>>(
    new Set()
  );
  const pollingTimersRef = useRef<Map<string, NodeJS.Timeout>>(new Map());

  // 获取模型列表
  const getModelList = async () => {
    try {
      if (!token) {
        return;
      }
      setModelListLoading(true);
      const response = await getTrainingRecordsApi({
        framework_type: 8,
        device_type: 40,
        page: 1,
        size: 100,
      });
      if (response.code == 0 || response.code == "0") {
        const data = response.data;
        // 合并 records 和 undone 数组
        const allModels = [...(data.records || []), ...(data.undone || [])];
        // 按 started_at 降序排序（最新的在前）
        allModels.sort((a, b) => {
          const timeA = parseInt(a.started_at || "0", 10);
          const timeB = parseInt(b.started_at || "0", 10);
          return timeB - timeA;
        });
        setModelList(allModels);

        // 检查是否有进行中的模型（status === "init" 且 progress > 0），启动轮询
        allModels.forEach((model) => {
          if (
            model.status === ModelConversionStatus.Init &&
            model.progress > 0
          ) {
            startPollingModelStatus(model.model_id);
          }
        });
      }
    } catch (error) {
      console.error("Get model list error:", error);
      setModelList([]);
    } finally {
      setModelListLoading(false);
    }
  };

  useEffect(() => {
    if (token) {
      getModelList();
    }
  }, [token]);

  // 清理所有轮询定时器
  useEffect(() => {
    const timers = pollingTimersRef.current;
    return () => {
      timers.forEach((timer) => {
        clearInterval(timer);
      });
      timers.clear();
    };
  }, []);

  // 处理模型上传
  const handleUploadModel = async (file: File) => {
    try {
      if (!token) {
        const errorMsg = "Please login first";
        messageApi.error(errorMsg);
        throw new Error(errorMsg);
      }

      setLoading(true);
      setLoadingTip("Uploading model and creating Model Conversion task");

      const response = await createTaskApi({
        framework_type: 8, // ONNX → CVIMODEL
        device_type: 40, // reCamera
        file: file,
        precision: "int8", // 默认使用 int8
        prompt: file.name, // 使用文件名作为 prompt
      });

      if (response.code === 0 || response.code === "0") {
        const modelId = response.data.model_id;
        messageApi.success(
          "Model upload successful, Model Conversion task created"
        );
        // 刷新模型列表
        await getModelList();
        // 开始轮询任务状态
        if (modelId) {
          startPollingModelStatus(modelId);
        }
      } else {
        const errorMsg = response.msg || "Upload failed";
        messageApi.error(errorMsg);
        throw new Error(errorMsg);
      }
    } catch (error: unknown) {
      console.error("Upload model error:", error);
      const errorMsg =
        error && typeof error === "object" && "msg" in error
          ? (error as { msg?: string }).msg
          : "Upload model failed";
      messageApi.error(errorMsg || "Upload model failed");
    } finally {
      setLoading(false);
      setLoadingTip("");
    }
  };

  // 处理文件上传前的验证
  const beforeUpload = (file: File) => {
    const isOnnx = file.name.toLowerCase().endsWith(".onnx");
    if (!isOnnx) {
      messageApi.error("Only .onnx files are allowed");
      return Upload.LIST_IGNORE;
    }
    return true;
  };

  // 开始轮询模型转换状态
  const startPollingModelStatus = (modelId: string) => {
    if (!token) {
      return;
    }

    // 如果已经有轮询在进行，先清除
    stopPollingModelStatus(modelId);

    const poll = async () => {
      try {
        const response = await getTrainStatusApi(modelId);

        if (response.code === 0 || response.code === "0") {
          const statusData = response.data;

          // 更新模型列表中的状态
          // getTrainStatusApi 返回的 status 可能是数字（表示进度），需要解析
          setModelList((prevList) => {
            return prevList.map((model) => {
              if (model.model_id === modelId) {
                let newStatus: ModelConversionStatus = model.status;
                let newProgress = model.progress;

                // 如果 status 是数字，表示进度，需要转换为 progress，status 保持为 init
                const statusValue = statusData.status;
                if (
                  typeof statusValue === "number" ||
                  (typeof statusValue === "string" &&
                    !isNaN(parseInt(statusValue, 10)) &&
                    statusValue !== ModelConversionStatus.Done &&
                    statusValue !== ModelConversionStatus.Error &&
                    statusValue !== ModelConversionStatus.Init)
                ) {
                  // status 是数字，表示进度，更新 progress，status 设置为 init
                  const statusNum =
                    typeof statusValue === "number"
                      ? statusValue
                      : parseInt(String(statusValue), 10);
                  if (!isNaN(statusNum)) {
                    newProgress = statusNum;
                    newStatus = ModelConversionStatus.Init;
                  }
                } else {
                  // status 是枚举值 "init"、"done" 或 "error"，直接使用
                  newStatus = statusValue as ModelConversionStatus;
                  // 如果是 done，progress 应该是 100；如果是 error，progress 不变；如果是 init，保持原有 progress
                  if (statusValue === ModelConversionStatus.Done) {
                    newProgress = 100;
                  } else if (statusValue === ModelConversionStatus.Init) {
                    newProgress = model.progress;
                  }
                }

                return {
                  ...model,
                  status: newStatus,
                  progress: newProgress,
                  error_message: statusData.error_message,
                  finished_at: statusData.finished_at || model.finished_at,
                };
              }
              return model;
            });
          });

          // 如果状态是 done 或 error，停止轮询
          // status 是数字或 "init" 时表示进行中，继续轮询
          const statusValue = statusData.status;
          const isDoneOrError =
            statusValue === ModelConversionStatus.Done ||
            statusValue === ModelConversionStatus.Error;
          if (isDoneOrError) {
            stopPollingModelStatus(modelId);
            // 刷新一次模型列表以获取最新数据
            getModelList();
          }
          // status 是数字或 "init" 时，继续轮询
        }
      } catch (error) {
        console.error("Polling model status error:", error);
        // 出错时停止轮询
        stopPollingModelStatus(modelId);
      }
    };

    // 立即执行一次
    poll();

    // 每 10 秒轮询一次
    const timer = setInterval(poll, 10000);
    pollingTimersRef.current.set(modelId, timer);
  };

  // 停止轮询模型转换状态
  const stopPollingModelStatus = (modelId: string) => {
    const timer = pollingTimersRef.current.get(modelId);
    if (timer) {
      clearInterval(timer);
      pollingTimersRef.current.delete(modelId);
    }
  };

  // 处理停止任务
  const handleStopTask = async (modelId: string) => {
    if (!token) {
      messageApi.error("Please login first");
      return;
    }

    const model = modelList.find((m) => m.model_id === modelId);
    const config = {
      title: `Are you sure to stop ${model?.prompt || "this task"}`,
      content: (
        <div>
          <p className="mb-16">
            You are about to stop the Model Conversion task. This action cannot
            be undone.
          </p>
          <p>
            The task will be stopped and you will not be able to download the
            converted model. Are you sure you want to continue?
          </p>
        </div>
      ),
      okButtonProps: {
        type: "primary",
        danger: true,
      } as ButtonProps,
      okText: "Stop",
    };
    const confirmed = await modal.confirm(config);
    if (confirmed) {
      // 设置 loading 状态
      setStoppingTaskIds((prev) => new Set(prev).add(modelId));
      try {
        const response = await stopTaskApi(modelId);
        if (response.code === 0 || response.code === "0") {
          messageApi.success("Task stopped successfully");
          // 停止轮询
          stopPollingModelStatus(modelId);
          // 刷新模型列表
          await getModelList();
        } else {
          messageApi.error(response.msg || "Stop task failed");
        }
      } catch (error: unknown) {
        console.error("Stop task error:", error);
        const errorMsg =
          error && typeof error === "object" && "msg" in error
            ? (error as { msg?: string }).msg
            : "Stop task failed";
        messageApi.error(errorMsg || "Stop task failed");
      } finally {
        // 清除 loading 状态
        setStoppingTaskIds((prev) => {
          const next = new Set(prev);
          next.delete(modelId);
          return next;
        });
      }
    }
  };

  // 处理模型删除
  const handleDeleteModel = async (modelId: string) => {
    if (!token) {
      messageApi.error("Please login first");
      return;
    }

    const model = modelList.find((m) => m.model_id === modelId);
    const config = {
      title: `Are you sure to delete ${model?.prompt || "this model"}`,
      content: (
        <div>
          <p className="mb-16">
            You are about to permanently delete this model. This action cannot
            be undone.
          </p>
          <p>
            Please ensure that you have backed up any essential information
            before proceeding. Are you sure you want to continue with the
            deletion?
          </p>
        </div>
      ),
      okButtonProps: {
        type: "primary",
        danger: true,
      } as ButtonProps,
      okText: "Delete",
    };
    const confirmed = await modal.confirm(config);
    if (confirmed) {
      try {
        const response = await deleteModelApi(modelId);
        if (response.code === 0 || response.code === "0") {
          messageApi.success("Model deleted successfully");
          await getModelList();
        } else {
          messageApi.error(response.msg || "Delete failed");
        }
      } catch (error: unknown) {
        console.error("Delete model error:", error);
        const errorMsg =
          error && typeof error === "object" && "msg" in error
            ? (error as { msg?: string }).msg
            : "Delete model failed";
        messageApi.error(errorMsg || "Delete model failed");
      }
    }
  };

  // 下载模型（公共逻辑）
  const downloadModel = async (modelId: string): Promise<Blob | null> => {
    if (!token) {
      messageApi.error("Please login first");
      return null;
    }

    try {
      setLoading(true);
      setLoadingTip("Downloading model");
      const blob = await getModelApi(modelId);
      return blob;
    } catch (error: unknown) {
      console.error("Download model error:", error);
      const errorMsg =
        error && typeof error === "object" && "msg" in error
          ? (error as { msg?: string }).msg
          : "Download model failed";
      messageApi.error(errorMsg || "Download model failed");
      return null;
    } finally {
      setLoading(false);
      setLoadingTip("");
    }
  };

  // 发送 flow 到 Node-RED
  const sendFlow = async (flows?: string) => {
    try {
      const revision = await saveFlows(flows);
      if (revision) {
        const response = await getFlowsState();
        if (response?.state == "stop") {
          await setFlowsState({ state: "start" });
        }
      }
    } catch (error) {
      console.error("Send flow error:", error);
    }
  };

  // 处理模型下载到本地
  const handleDownloadModel = async (modelId: string, prompt: string) => {
    const blob = await downloadModel(modelId);
    if (!blob) return;

    try {
      // 创建下载链接
      const url = window.URL.createObjectURL(blob);
      const a = document.createElement("a");
      a.href = url;
      a.download = `${prompt || modelId}.cvimodel`;
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      window.URL.revokeObjectURL(url);

      messageApi.success("Model downloaded successfully");
    } catch (error: unknown) {
      console.error("Save model to local error:", error);
      messageApi.error("Save model to local failed");
    }
  };

  // 处理模型下载并上传到设备
  const handleDownloadAndUploadModel = async (
    modelId: string,
    prompt: string
  ) => {
    if (!deviceInfo?.ip) {
      messageApi.error("Device not connected");
      return;
    }

    const blob = await downloadModel(modelId);
    if (!blob) return;

    try {
      setLoading(true);
      setLoadingTip("Uploading model to device");
      const formData = new FormData();
      formData.append("model_file", blob, `${prompt || modelId}.cvimodel`);

      // 创建简单的模型信息
      const modelInfo = {
        model_id: modelId,
        model_name: prompt || modelId,
      };
      formData.append("model_info", JSON.stringify(modelInfo));

      // 使用分片上传并显示进度
      const resp = await uploadModelApi(formData, (progress) => {
        setLoadingTip(`Uploading model to device: ${progress.toFixed(1)}%`);
      });
      
      if (resp.code == 0) {
        messageApi.success("Model uploaded to device successfully");

        // 更新 flow 以使 Node-RED 生效
        setLoadingTip("Updating flow");
        try {
          const flows = await getFlows();
          if (flows && Array.isArray(flows)) {
            // 找到 type 为 "model" 的节点，将其 uri 设置为空字符串
            const updatedFlows = flows.map((node: Record<string, unknown>) => {
              if (node.type === "model") {
                return {
                  ...node,
                  uri: "",
                };
              }
              return node;
            });
            const flowsStr = JSON.stringify(updatedFlows);
            await sendFlow(flowsStr);
          }
        } catch (error) {
          console.error("Update flow error:", error);
          // 不显示错误，因为模型已经上传成功
        }
      } else {
        messageApi.error("Upload model to device failed");
      }
    } catch (error: unknown) {
      console.error("Upload model to device error:", error);
      const errorMsg =
        error && typeof error === "object" && "msg" in error
          ? (error as { msg?: string }).msg
          : "Upload model to device failed";
      messageApi.error(errorMsg || "Upload model to device failed");
    } finally {
      setLoading(false);
      setLoadingTip("");
    }
  };

  // 格式化日期
  const formatDate = (timestamp: string) => {
    if (!timestamp) return "";
    const date = new Date(parseInt(timestamp, 10));
    const year = date.getFullYear();
    const month = String(date.getMonth() + 1).padStart(2, "0");
    const day = String(date.getDate()).padStart(2, "0");
    const hours = String(date.getHours()).padStart(2, "0");
    const minutes = String(date.getMinutes()).padStart(2, "0");
    return `${year}-${month}-${day} ${hours}:${minutes}`;
  };

  // 获取状态图标
  const getStatusIcon = (status: ModelConversionStatus) => {
    if (status === ModelConversionStatus.Done) {
      return <CheckCircleOutlined className="text-primary text-20" />;
    } else if (status === ModelConversionStatus.Error) {
      return <CloseCircleOutlined className="text-[#ff4d4f] text-20" />;
    } else {
      // init 状态和数值状态（进行中）都使用 LoadingOutlined
      return <LoadingOutlined className="text-primary text-20" spin />;
    }
  };

  // 格式化模型状态
  // status 只可能是枚举值（init、done、error），progress 是独立的进度字段
  const formatModelStatus = (
    status: ModelConversionStatus,
    progress?: number
  ) => {
    if (status === ModelConversionStatus.Done) {
      return "Done";
    } else if (status === ModelConversionStatus.Error) {
      return "Error";
    } else {
      // init 状态时使用 progress 字段表达进度，如果没有值则显示 0
      return `${progress ?? 0}%`;
    }
  };

  return (
    <div className="flex flex-col h-full">
      <div className="mb-12">
        <Upload
          accept=".onnx"
          beforeUpload={beforeUpload}
          customRequest={({ file, onSuccess, onError }) => {
            if (file instanceof File) {
              handleUploadModel(file)
                .then(() => {
                  onSuccess?.(file);
                })
                .catch((error) => {
                  onError?.(error);
                });
            }
          }}
          showUploadList={false}
        >
          <Button type="primary" icon={<UploadOutlined />}>
            Upload Model
          </Button>
        </Upload>
      </div>

      <div className="flex flex-col flex-1">
        <Spin spinning={modelListLoading}>
          <div className={styles.listContainer}>
            {modelList?.length > 0 ? (
              modelList.map((model, index) => (
                <div
                  key={index}
                  className="bg-background border border-gray-200 rounded-4 p-12 mb-12"
                >
                  <div className="mb-8">
                    <div className="flex justify-between items-center mb-6">
                      <div className="font-semibold text-16 text-gray-800">
                        {model.prompt}
                      </div>
                      <div>{getStatusIcon(model.status)}</div>
                    </div>
                    <div className="flex justify-between items-center text-12 text-gray-600">
                      <span>
                        {model.started_at
                          ? `Uploaded: ${formatDate(model.started_at)}`
                          : ""}
                      </span>
                      {(model.status !== ModelConversionStatus.Done &&
                        model.status !== ModelConversionStatus.Error &&
                        model.status !== ModelConversionStatus.Init) ||
                      (model.status === ModelConversionStatus.Init &&
                        model.progress > 0) ? (
                        <span>
                          {formatModelStatus(model.status, model.progress)}
                        </span>
                      ) : null}
                    </div>
                  </div>
                  <div className="mt-8 flex justify-between items-center">
                    <div className="flex gap-8">
                      {model.status === ModelConversionStatus.Done && (
                        <>
                          <Button
                            type="default"
                            size="small"
                            onClick={() =>
                              handleDownloadModel(model.model_id, model.prompt)
                            }
                          >
                            Download
                          </Button>
                          <Button
                            type="primary"
                            size="small"
                            onClick={() =>
                              handleDownloadAndUploadModel(
                                model.model_id,
                                model.prompt
                              )
                            }
                          >
                            Use
                          </Button>
                        </>
                      )}
                      {model.status === ModelConversionStatus.Error && (
                        <Button
                          danger
                          size="small"
                          onClick={() => {
                            Modal.info({
                              title: "Error Details",
                              width: 600,
                              content: (
                                <div
                                  style={{
                                    maxHeight: "400px",
                                    overflowY: "auto",
                                    whiteSpace: "pre-wrap",
                                    wordBreak: "break-word",
                                  }}
                                >
                                  {model.error_message ||
                                    "No error details available"}
                                </div>
                              ),
                            });
                          }}
                        >
                          View Error
                        </Button>
                      )}
                      {/* Loading 状态时显示停止按钮 */}
                      {(model.status === ModelConversionStatus.Init ||
                        (model.status !== ModelConversionStatus.Done &&
                          model.status !== ModelConversionStatus.Error)) && (
                        <Button
                          danger
                          size="small"
                          loading={stoppingTaskIds.has(model.model_id)}
                          disabled={stoppingTaskIds.has(model.model_id)}
                          onClick={() => handleStopTask(model.model_id)}
                        >
                          Stop Model Conversion
                        </Button>
                      )}
                    </div>
                    {/* Loading 状态时隐藏删除按钮，done/error 状态时显示 */}
                    {(model.status === ModelConversionStatus.Done ||
                      model.status === ModelConversionStatus.Error) && (
                      <Tooltip title="Delete model">
                        <Button
                          danger
                          size="small"
                          icon={<DeleteOutlined />}
                          onClick={() => handleDeleteModel(model.model_id)}
                        />
                      </Tooltip>
                    )}
                  </div>
                </div>
              ))
            ) : (
              <div className="text-text text-12 text-center py-20">
                No models yet. Upload an ONNX model to get started.
              </div>
            )}
          </div>
        </Spin>
      </div>
    </div>
  );
};

export default ModelConversion;
