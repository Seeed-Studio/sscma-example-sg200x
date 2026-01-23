import { useState, useEffect, useRef } from "react";
import { Modal, Button, Input, message, ButtonProps, Tooltip } from "antd";
import {
  PlusCircleOutlined,
  SyncOutlined,
  EditOutlined,
  DeleteOutlined,
} from "@ant-design/icons";
import {
  getAppListApi,
  updateAppApi,
  deleteAppApi,
  viewAppApi,
  removeFileApi,
} from "@/api/sensecraft";
import { IAppInfo } from "@/api/sensecraft/sensecraft";
import usePlatformStore from "@/store/platform";

import { IModelData } from "@/api/sensecraft/sensecraft";
import styles from "./index.module.css";

interface ApplicationListProps {
  onSelectApp: (app_id: string) => Promise<void>;
  onSyncApp: (app_id: string) => Promise<void>;
  onCreateAppAndUpdateFlow: (params: {
    app_name?: string;
    flow_data?: string;
    model_data?: IModelData | null;
    needUpdateFlow?: boolean;
    needUpdateApp?: boolean;
  }) => Promise<boolean>;
  syncing: boolean;
  messageApi: ReturnType<typeof message.useMessage>[0];
  modal: ReturnType<typeof Modal.useModal>[0];
}

const ApplicationList = ({
  onSelectApp,
  onSyncApp,
  onCreateAppAndUpdateFlow,
  syncing,
  messageApi,
  modal,
}: ApplicationListProps) => {
  const { appInfo } = usePlatformStore();
  const [applist, setApplist] = useState<IAppInfo[]>([]);
  const [, setAppListLoading] = useState(false);
  const [focusAppid, setFocusAppid] = useState("");
  const inputRef = useRef<string>("");

  // 获取应用列表
  const getAppList = async () => {
    try {
      setAppListLoading(true);
      const response = await getAppListApi();
      if (response.code == 0) {
        const data = response.data;
        const list = data.list;
        setApplist(list);
      }
    } catch (error) {
      setApplist([]);
    } finally {
      setAppListLoading(false);
    }
  };

  // 获取应用详情
  const getAppInfo = async (appid?: string) => {
    try {
      if (!appid) {
        return null;
      }
      const response = await viewAppApi(appid);
      if (response.code == 0) {
        return response.data;
      }
      return null;
    } catch (error) {
      return null;
    }
  };

  useEffect(() => {
    getAppList();
  }, []);

  const handleCreateApp = async () => {
    try {
      inputRef.current = "";
      const config = {
        title: "New Application",
        content: (
          <Input
            placeholder="Please input application name"
            maxLength={32}
            defaultValue={inputRef.current}
            onChange={handleInputNameChange}
          />
        ),
      };
      const confirmed = await modal.confirm(config);
      if (confirmed) {
        await onCreateAppAndUpdateFlow({
          app_name: inputRef.current,
          needUpdateFlow: true,
        });
        await getAppList();
      }
    } catch (error) {
      messageApi.error("Create app failed");
    }
  };

  const handleEditApp = async (appId: string) => {
    try {
      const currApp = applist.find((app) => app.app_id == appId);
      if (!currApp) {
        return;
      }
      inputRef.current = currApp.app_name;
      const config = {
        title: "Edit Application name",
        content: (
          <Input
            placeholder="Please input application name"
            maxLength={32}
            defaultValue={inputRef.current}
            onChange={handleInputNameChange}
          />
        ),
      };
      const confirmed = await modal.confirm(config);
      if (confirmed) {
        const response = await updateAppApi({
          app_id: appId,
          app_name: inputRef.current,
        });
        if (response.code == 0) {
          currApp.app_name = inputRef.current;
          setApplist([...applist]);
        }
      }
    } catch (error) {
      console.log(error);
    }
  };

  const handleDeleteApp = async (app_id: string) => {
    const app = await getAppInfo(app_id);
    if (!app) {
      messageApi.error("App is not exist");
      getAppList();
      return;
    }
    const config = {
      title: `Are you sure to delete ${app.app_name}`,
      content: (
        <div>
          <p className="mb-16">
            You are about to permanently delete this project. This action cannot
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
      const response = await deleteAppApi({ app_id: app.app_id });
      if (response.code == 0) {
        const model_data = app.model_data;
        //如果是本地模型。则把本地模型在云端的存储也删掉
        if (model_data?.model_id == "0") {
          const model_name = model_data.model_name;
          const file_name = `${model_name}_${app.app_id}`;
          //异步删除就好，不需要考虑结果
          removeFileApi(file_name);
        }

        const updatedAppList = applist.filter(
          (item) => item.app_id !== app.app_id
        );
        setApplist(updatedAppList);
        // 如果删除的是当前应用则切换为第一个应用
        if (app.app_id == appInfo?.app_id) {
          onSyncApp(updatedAppList[0]?.app_id);
        }
      }
    }
  };

  const handleSelectApp = async (app_id: string) => {
    await onSelectApp(app_id);
  };

  const handleInputNameChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    inputRef.current = e.target.value;
  };

  const handleFocusApp = async (appId: string) => {
    setFocusAppid(appId);
  };

  const handleBlurApp = async () => {
    setFocusAppid("");
  };

  return (
    <div className="flex flex-col h-full">
      <div className="mb-12">
        <Button
          type="primary"
          icon={<PlusCircleOutlined />}
          onClick={handleCreateApp}
        >
          New Application
        </Button>
      </div>

      <div className="flex flex-col flex-1">
        <div className="text-text text-12 mb-10">
          reCamera can only run 1 application at a time
        </div>
        <div className={styles.listContainer}>
          {applist?.length > 0 &&
            applist.map((app, index) =>
              appInfo?.app_id == app.app_id ? (
                <div key={index} className="flex mb-10 mr-6 h-40 min-w-0">
                  <div
                    className="flex flex-1 min-w-0 text-14 text-primary border border-primary rounded-4 p-8 overflow-hidden"
                    onMouseEnter={() => handleFocusApp(app.app_id)}
                    onMouseLeave={() => handleBlurApp()}
                  >
                    <Tooltip title={app.app_name}>
                      <div
                        className="flex flex-1 min-w-0 font-medium cursor-pointer overflow-hidden text-ellipsis whitespace-nowrap"
                        onClick={() => handleSelectApp(app.app_id)}
                      >
                        {app.app_name}
                      </div>
                    </Tooltip>
                    <div
                      className={`flex shrink-0 ${
                        focusAppid == app.app_id ? "flex" : "hidden"
                      }`}
                    >
                      <EditOutlined
                        className="mr-5 ml-5 pt-5 pb-5"
                        onClick={() => handleEditApp(app.app_id)}
                      />
                      {applist.length > 1 && (
                        <DeleteOutlined
                          className="pt-5 pb-5"
                          onClick={() => handleDeleteApp(app.app_id)}
                        />
                      )}
                    </div>
                  </div>

                  {syncing && (
                    <SyncOutlined
                      className="text-disable ml-10 mr-15 text-20"
                      spin
                    />
                  )}
                </div>
              ) : (
                <div
                  key={index}
                  className="flex mb-10 mr-6 h-30 min-w-0 text-12 bg-background text-text border border-disable rounded-4 px-8 overflow-hidden"
                  onMouseEnter={() => handleFocusApp(app.app_id)}
                  onMouseLeave={() => handleBlurApp()}
                >
                  <Tooltip title={app.app_name}>
                    <div
                      className="flex flex-1 min-w-0 items-center cursor-pointer overflow-hidden text-ellipsis whitespace-nowrap"
                      onClick={() => handleSelectApp(app.app_id)}
                    >
                      {app.app_name}
                    </div>
                  </Tooltip>
                  <div
                    className={`flex shrink-0 ${
                      focusAppid == app.app_id ? "flex" : "hidden"
                    }`}
                  >
                    <EditOutlined
                      className="mr-5 ml-5 pt-5 pb-5"
                      onClick={() => handleEditApp(app.app_id)}
                    />
                    {applist.length > 1 && (
                      <DeleteOutlined
                        className="pt-5 pb-5"
                        onClick={() => handleDeleteApp(app.app_id)}
                      />
                    )}
                  </div>
                </div>
              )
            )}
        </div>
      </div>
    </div>
  );
};

export default ApplicationList;
