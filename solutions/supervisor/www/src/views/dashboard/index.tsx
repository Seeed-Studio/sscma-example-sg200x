import { useState, useEffect, useRef, useMemo } from "react";
import { useNavigate } from "react-router-dom";
import { Button, FloatButton, Spin } from "antd";
import { ServiceStatus } from "@/enum";
import { baseIP } from "@/utils/noderedRequest";
import { queryServiceStatusApi } from "@/api/device";
import DashboardImg from "@/assets/images/svg/dashboard.svg";

const Dashboard = () => {
  const navigate = useNavigate();
  const [hasDashboard, setHasDashboard] = useState(true);
  const [serviceStatus, setServiceStatus] = useState<ServiceStatus>(
    ServiceStatus.STARTING
  );
  const tryCount = useRef<number>(0);

  useEffect(() => {
    queryServiceStatus();
  }, []);

  const queryServiceStatus = async () => {
    tryCount.current = 0;
    setServiceStatus(ServiceStatus.STARTING);

    while (tryCount.current < 30) {
      try {
        const response = await queryServiceStatusApi();

        if (response.code === 0 && response.data) {
          const { sscmaNode, nodeRed, system } = response.data;

          if (
            sscmaNode === ServiceStatus.RUNNING &&
            nodeRed === ServiceStatus.RUNNING &&
            system === ServiceStatus.RUNNING
          ) {
            setServiceStatus(ServiceStatus.RUNNING);
            queryDashboard();
            return;
          }
        }

        tryCount.current++;
        setServiceStatus(ServiceStatus.STARTING);
        await new Promise((resolve) => setTimeout(resolve, 5000));
      } catch (error) {
        tryCount.current++;
        console.error("Error querying service status:", error);
      }
    }

    setServiceStatus(ServiceStatus.FAILED);
  };

  const queryDashboard = async () => {
    const dashboardRes = await fetch(dashboardURL);
    if (
      dashboardRes.ok &&
      (dashboardRes.status == 200 || dashboardRes.status == 204)
    ) {
      setHasDashboard(true);
    } else {
      setHasDashboard(false);
    }
  };

  const dashboardURL = useMemo(() => {
    return `${baseIP}:1880/dashboard`;
  }, [baseIP]);

  const gotoSensecraft = () => {
    window.open("https://sensecraft.seeed.cc/ai/#/recamera", "_blank");
  };

  const gotoWorkspace = () => {
    navigate("/workspace");
  };
  return (
    <div className="w-full h-full">
      {baseIP && serviceStatus == ServiceStatus.RUNNING ? (
        <div className="w-full h-full flex justify-center items-center">
          {hasDashboard ? (
            <div className="w-full h-full relative overflow-hidden">
              <iframe className="w-full h-full" src={dashboardURL} />
              <FloatButton
                type="primary"
                icon={<img src={DashboardImg} />}
                onClick={gotoWorkspace}
              />
            </div>
          ) : (
            <div className="flex flex-col justify-center items-center">
              <div className="text-16 mb-10">
                Something went wrong with dashboard, please check your palette
                and flow
              </div>
              <Button
                style={{ fontSize: "16px" }}
                type="link"
                onClick={gotoSensecraft}
              >
                Download dashboard flow
              </Button>
              <div className="text-16 mb-10">or</div>
              <Button type="primary" onClick={gotoWorkspace}>
                Go to Workspace
              </Button>
            </div>
          )}
        </div>
      ) : (
        <div className="w-full h-full flex justify-center items-center">
          {serviceStatus == ServiceStatus.STARTING && (
            <Spin tip="Loading"></Spin>
          )}
          {(serviceStatus == ServiceStatus.ERROR ||
            serviceStatus == ServiceStatus.FAILED) && (
            <div className="flex flex-col justify-center items-center">
              <div className="text-16 mb-10">
                Service startup failed, please check your device
              </div>
              <Button type="primary" onClick={gotoWorkspace}>
                Go to Workspace
              </Button>
            </div>
          )}
        </div>
      )}
    </div>
  );
};

export default Dashboard;
