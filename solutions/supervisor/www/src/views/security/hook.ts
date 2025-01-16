import { useEffect, Reducer, useReducer, useRef } from "react";
import { Toast } from "antd-mobile";
import { FormInstance } from "antd-mobile/es/components/form";
import useConfigStore from "@/store/config";
import { encryptPassword } from "@/utils";
import moment from "moment";
import {
  getUserInfoApi,
  updateUserInfoApi,
  updateUserPasswordApi,
  delSshKeyApi,
  addSshKeyApi,
} from "@/api/user";
import useUserStore from "@/store/user";

interface FormParams {
  username: string;
  oldPassword: string;
  newPassword: string;
  sshName: string;
  sshKey: string;
}
export enum IFormTypeEnum {
  Key = "Key",
  Username = "Username",
  Password = "Password",
  DelKey = "DelKey",
}
interface IInitialState {
  visible: boolean;
  formType: IFormTypeEnum;
  password: string;
  form: Partial<FormParams>;
  username: string;
  sshkeyList: ISshItem[];
  curSshInfo?: ISshItem;
}
type ActionType = { type: "setState"; payload: Partial<IInitialState> };
const initialState: IInitialState = {
  visible: false,
  formType: IFormTypeEnum.Key,
  password: "",
  form: {},
  username: "",
  sshkeyList: [],
};
function reducer(state: IInitialState, action: ActionType): IInitialState {
  switch (action.type) {
    case "setState":
      return {
        ...state,
        ...action.payload,
      };
    default:
      throw new Error();
  }
}
export function useData() {
  // ---状态管理
  const [state, stateDispatch] = useReducer<Reducer<IInitialState, ActionType>>(
    reducer,
    initialState
  );
  const { deviceInfo } = useConfigStore();
  const { updatePassword } = useUserStore();
  const setStates = (payload: Partial<IInitialState>) => {
    stateDispatch({ type: "setState", payload: payload });
  };
  const passwordFormRef = useRef<FormInstance>(null);
  const usernameFormRef = useRef<FormInstance>(null);
  const formRef = useRef<FormInstance>(null);

  const onQueryUserInfo = async () => {
    try {
      const { data } = await getUserInfoApi();
      setStates({
        username: data.userName,
        sshkeyList: data.sshkeyList,
      });
    } catch (err) {}
  };
  const onCancel = () => {
    setStates({
      visible: false,
    });
    switch (state.formType) {
      case IFormTypeEnum.Key:
        formRef.current?.resetFields();

        break;
      case IFormTypeEnum.Password:
        passwordFormRef.current?.resetFields();
        break;
      case IFormTypeEnum.Username:
        usernameFormRef.current?.resetFields();
        break;
    }
  };
  const onEdit = (type: IFormTypeEnum) => {
    setStates({
      visible: true,
      formType: type,
    });
  };

  const addSshKey = () => {
    setStates({
      visible: true,
      formType: IFormTypeEnum.Key,
    });
  };

  const onPasswordFinish = async (values: FormParams) => {
    const encryptedOldpassword = encryptPassword(values.oldPassword);
    const encryptedNewPassword = encryptPassword(values.newPassword || "");
    const response = await updateUserPasswordApi({
      oldPassword: encryptedOldpassword,
      newPassword: encryptedNewPassword,
    });
    if (response.code == 0) {
      updatePassword(encryptedNewPassword);
    }
    refresh();
  };
  const onAddSshFinish = async (values: FormParams) => {
    await addSshKeyApi({
      name: values.sshName,
      value: values.sshKey || "",
      time: moment().format("YYYY-MM-DD"),
    });
    refresh();
  };
  const refresh = () => {
    onCancel();
    onQueryUserInfo();
  };
  const onUsernameFinish = async (values: FormParams) => {
    await updateUserInfoApi({
      userName: values.username || "",
    });
    refresh();
  };

  const onDelete = (item?: ISshItem) => {
    setStates({
      visible: true,
      formType: IFormTypeEnum.DelKey,
      curSshInfo: item,
    });
  };
  const onDeleteFinish = async () => {
    await delSshKeyApi({ id: state.curSshInfo?.id || "" });
    onQueryUserInfo();
    setStates({
      visible: false,
    });
  };
  useEffect(() => {
    onQueryUserInfo();
  }, []);
  return {
    state,
    setStates,
    formRef,
    deviceInfo,
    passwordFormRef,
    usernameFormRef,
    onCancel,
    onEdit,
    addSshKey,
    onPasswordFinish,
    onUsernameFinish,
    onDelete,
    onAddSshFinish,
    onDeleteFinish,
  };
}