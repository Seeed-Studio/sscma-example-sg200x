import { useEffect, useRef } from "react";

const NodeRed = ({
  ip,
  timestamp = 1,
  onReceivedDeployData,
}: {
  ip: string;
  timestamp: number;
  onReceivedDeployData?: (revision: string | null | undefined) => void;
}) => {
  const onReceivedDeployDataRef = useRef(onReceivedDeployData);

  useEffect(() => {
    onReceivedDeployDataRef.current = onReceivedDeployData;
  }, [onReceivedDeployData]);

  const onReceivedMessage = (data: any) => {
    try {
      const message = JSON.parse(data);
      // console.log("Received message:", message);
      // 查找topic为"notification/runtime-deploy"的消息
      const deployData =
        message.find(
          (item: { topic: string }) =>
            item.topic === "notification/runtime-deploy"
        )?.data || null;

      if (deployData) {
        const revision = deployData.revision;
        onReceivedDeployDataRef.current?.(revision);
      }
    } catch (error) {}
  };

  useEffect(() => {
    const socket = new WebSocket(`ws://${ip}:1880/comms`);

    socket.onopen = () => {
      console.log("WebSocket connection established");
    };

    socket.onmessage = (event) => {
      onReceivedMessage(event.data);
    };

    socket.onerror = (error) => {
      console.error("WebSocket error:", error);
    };

    socket.onclose = () => {
      console.log("WebSocket connection closed");
    };

    // Clean up the WebSocket connection when the component is unmounted
    return () => {
      socket.close();
    };
  }, [ip]);

  return (
    <iframe
      className="w-full h-full"
      src={`http://${ip}:1880?timestamp=${timestamp}`}
    />
  );
};

export default NodeRed;
