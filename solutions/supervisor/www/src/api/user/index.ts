import { supervisorRequest } from "@/utils/request";
// Get user info
export const getUserInfoApi = async () =>
  supervisorRequest<IUserInfo>({
    url: "api/userMgr/queryUserInfo",
    method: "get",
  });
// Update user info
export const updateUserInfoApi = async (data: { userName: string }) =>
  supervisorRequest({
    url: "api/userMgr/updateUserName",
    method: "post",
    data,
  });
// Update user password
export const updateUserPasswordApi = async (data: {
  oldPassword: string;
  newPassword: string;
}) =>
  supervisorRequest({
    url: "api/userMgr/updatePassword",
    method: "post",
    data,
  });
// Login
export const loginApi = async (data: { userName: string; password: string }) =>
  supervisorRequest<{ token: string; retryCount?: number }>(
    {
      url: "api/userMgr/login",
      method: "post",
      data,
    },
    {
      catchs: true,
    }
  );
// Set SSH status
export const setSShStatusApi = async (data: {
  enabled: boolean;
}) =>
  supervisorRequest({
    url: "api/userMgr/setSShStatus",
    method: "post",
    data,
  });
// Add SSH key
export const addSshKeyApi = async (data: {
  name: string;
  value: string;
  time: string;
}) =>
  supervisorRequest({
    url: "api/userMgr/addSShkey",
    method: "post",
    data,
  });
// Delete SSH key
export const delSshKeyApi = async (data: { id: string }) =>
  supervisorRequest({
    url: "api/userMgr/deleteSShkey",
    method: "post",
    data,
  });
