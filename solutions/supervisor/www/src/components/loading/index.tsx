import { useEffect, useState, useRef } from "react";
import { ServiceStatus } from "@/enum";
import { Progress, Alert } from "antd";
import GifParse from "gif-parser-web";
import { queryServiceStatusApi } from "@/api/device";
import gif1 from "@/assets/gif/gif1.gif";
import gif2 from "@/assets/gif/gif2.gif";
import gif3 from "@/assets/gif/gif3.gif";
import gif4 from "@/assets/gif/gif4.gif";
import gif5 from "@/assets/gif/gif5.gif";

const gifs = [gif1, gif2, gif3, gif4, gif5];
const totalDuration = 100 * 1000; // 100秒

const Loading = ({
  onServiceStatusChange,
}: {
  onServiceStatusChange?: (serviceStatus: ServiceStatus) => void;
}) => {
  const [serviceStatus, setServiceStatus] = useState<ServiceStatus>(
    ServiceStatus.STARTING
  );
  const [currentGifIndex, setCurrentGifIndex] = useState(0);
  const [progress, setProgress] = useState(0);
  const tryCount = useRef<number>(0);

  useEffect(() => {
    let timer: number;

    const fetchGifInfo = async () => {
      if (serviceStatus != ServiceStatus.STARTING) {
        return;
      }
      try {
        const gifParse = new GifParse(gifs[currentGifIndex]);
        const gifInfo = await gifParse.getInfo();
        const duration = gifInfo.duration;

        // 设置定时器，按顺序切换到下一个 GIF
        timer = window.setTimeout(() => {
          setCurrentGifIndex((prevIndex) => (prevIndex + 1) % gifs.length);
        }, duration);
      } catch (error) {
        console.error("parse gif error:", error);
      }
    };

    fetchGifInfo();

    return () => {
      clearTimeout(timer); // 清理定时器
    };
  }, [currentGifIndex, serviceStatus]);

  useEffect(() => {
    onServiceStatusChange?.(serviceStatus);
  }, [serviceStatus]);

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
          const { sscmaNode, nodeRed, system, uptime = 0 } = response.data;
          if (
            sscmaNode === ServiceStatus.RUNNING &&
            nodeRed === ServiceStatus.RUNNING &&
            system === ServiceStatus.RUNNING
          ) {
            setProgress(100);
            setServiceStatus(ServiceStatus.RUNNING);
            return;
          } else {
            let percentage = (uptime / totalDuration) * 100;
            percentage = percentage >= 100 ? 99.99 : percentage;
            setProgress(parseFloat(percentage.toFixed(2)));
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

  return (
    <div className="flex justify-center items-center absolute left-0 top-0 right-0 bottom-0 z-100 bg-[#f1f3f5]">
      {serviceStatus == ServiceStatus.STARTING && (
        <div className="w-1/2">
          <img src={gifs[currentGifIndex]} />
          {progress > 0 && (
            <Progress
              className="p-12 mt-36"
              percent={progress}
              strokeColor={"#8FC31F"}
            />
          )}
          <div className="text-16 mx-12 text-center">
            Please wait for node-red service to get start, it takes around 1 and
            an half minutes.
          </div>
        </div>
      )}
      {serviceStatus == ServiceStatus.FAILED && (
        <div className="w-1/2">
          <Alert
            message="System Error"
            description={
              <span>
                Looks like something went wrong with system. Please check system
                and restart, or contact{" "}
                <a
                  href="mailto:techsupport@seeed.io"
                  style={{ color: "#4096ff" }}
                >
                  techsupport@seeed.io
                </a>{" "}
                for support.
              </span>
            }
            type="error"
            showIcon
          />
        </div>
      )}
    </div>
  );
};

export default Loading;
