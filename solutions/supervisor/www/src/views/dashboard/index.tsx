import { useState, useEffect, useMemo } from "react";
import { useNavigate } from "react-router-dom";
import { Button, FloatButton } from "antd";
import { baseIP } from "@/utils/noderedRequest";
import DashboardImg from "@/assets/images/svg/dashboard.svg";
import { getFlows } from "@/api/nodered";

const Dashboard = () => {
  const navigate = useNavigate();
  const [hasDashboard, setHasDashboard] = useState(true);

  useEffect(() => {
    queryDashboard();
  }, []);

  const queryDashboard = async () => {
    try {
      const dashboardRes = await fetch(dashboardURL);
      if (
        dashboardRes.ok &&
        (dashboardRes.status == 200 || dashboardRes.status == 204)
      ) {
        const localFlowsData = await getFlows();
        let hasUINode =
          localFlowsData?.some((node: any) => node.type === "ui-base") ?? false;
        setHasDashboard(hasUINode);
      } else {
        setHasDashboard(false);
      }
    } catch (error) {
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
              Something went wrong with dashboard, please check your palette and
              flow
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
    </div>
  );
};

export default Dashboard;
