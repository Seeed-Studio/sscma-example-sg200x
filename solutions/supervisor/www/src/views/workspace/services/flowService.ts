import { queryServiceStatusApi } from "@/api/device";
import { getFlowsState, saveFlows, setFlowsState } from "@/api/nodered";
import { ServiceStatus } from "@/enum";

export const FLOW_DEPLOY_EMPTY_REVISION_ERROR =
  "Node-RED did not accept flow deployment (empty revision)";

const sleep = (ms: number) =>
  new Promise((resolve) => setTimeout(resolve, ms));

export const summarizeFlow = (flow?: string) => {
  const summary = {
    len: flow?.length ?? 0,
    nodes: null as number | null,
    hasUi: false,
    hasUiTab: false,
    hasTab: false,
    hasModel: false,
    typesSample: [] as string[],
  };
  if (!flow) return summary;

  try {
    const data = JSON.parse(flow);
    if (Array.isArray(data)) {
      summary.nodes = data.length;
      const types = new Set<string>();
      for (const node of data) {
        const type = (node as { type?: string })?.type || "";
        if (type) types.add(type);
        if (type.startsWith("ui-")) summary.hasUi = true;
        if (type === "ui-tab") summary.hasUiTab = true;
        if (type === "tab") summary.hasTab = true;
        if (type === "model") summary.hasModel = true;
      }
      summary.typesSample = Array.from(types).slice(0, 8);
    }
  } catch (error) {
    // ignore parse errors for debug summary
  }

  return summary;
};

export const waitForDashboardReady = async (
  ip: string,
  timeoutMs = 30000,
  intervalMs = 1500
) => {
  const url = `http://${ip}:1880/dashboard`;
  const start = Date.now();

  while (Date.now() - start < timeoutMs) {
    try {
      const res = await fetch(url, { method: "GET" });
      if (res.ok && (res.status === 200 || res.status === 204)) {
        return true;
      }
    } catch (error) {
      // ignore and retry
    }
    await sleep(intervalMs);
  }
  return false;
};

export const waitForNodeRedReady = async (
  timeoutMs = 120000,
  intervalMs = 2000
) => {
  const start = Date.now();
  while (Date.now() - start < timeoutMs) {
    try {
      const response = await queryServiceStatusApi();
      if (
        response.code === 0 &&
        response.data &&
        response.data.nodeRed === ServiceStatus.RUNNING
      ) {
        return true;
      }
    } catch (error) {
      // ignore and retry
    }
    await sleep(intervalMs);
  }
  return false;
};

export const deployFlowToNodeRed = async (flows?: string) => {
  const revision = await saveFlows(flows);
  if (!revision) {
    throw new Error(FLOW_DEPLOY_EMPTY_REVISION_ERROR);
  }

  const response = await getFlowsState();
  if (response?.state == "stop") {
    await setFlowsState({ state: "start" });
  }

  return {
    revision,
    flowState: response?.state,
  };
};
