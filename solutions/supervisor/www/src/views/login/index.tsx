import {
  LockOutlined,
  UserOutlined,
} from "@ant-design/icons";
import { Button, Form, Input, Modal, message } from "antd";
import { useState } from "react";
import recameraLogo from "@/assets/images/recamera.png";
import useUserStore from "@/store/user";
import { requiredTrimValidate, passwordRules } from "@/utils/validate";
import { loginApi, updateUserPasswordApi } from "@/api/user";

const Login = () => {
  const { firstLogin, updateFirstLogin, updateUserInfo } = useUserStore();

  const [form] = Form.useForm();
  const [messageApi, messageContextHolder] = message.useMessage();
  const [passwordErrorMsg, setPasswordErrorMsg] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);

  const handleChangePassword = async () => {
    try {
      // Validate form first
      const fieldsValue = await form.validateFields();
      const oldpassword = fieldsValue.oldpassword;
      const newpassword = fieldsValue.newpassword;
      const confirmpassword = fieldsValue.confirmpassword;

      if (newpassword !== confirmpassword) {
        messageApi.error("New passwords do not match");
        return;
      }

      setLoading(true);
      const response = await updateUserPasswordApi({
        oldPassword: oldpassword,
        newPassword: newpassword,
      });

      if (response.code == 0) {
        messageApi.success("Password changed successfully");
        updateFirstLogin(false);
        form.resetFields();
      } else {
        messageApi.error(response.msg || "Password change failed");
      }
    } catch (error: unknown) {
      // Form validation failed - error will have fields that failed
      if (error && typeof error === 'object' && 'errorFields' in error) {
        // This is a validation error from antd
        console.log("Form validation failed:", error);
      } else {
        messageApi.error("An error occurred");
      }
    } finally {
      setLoading(false);
    }
  };

  const loginAction = async (userName: string, password: string) => {
    try {
      const response = await loginApi({
        userName,
        password,
      });
      const code = response.code;
      const data = response.data;
      if (code === 0) {
        updateUserInfo({
          userName,
          password,
          token: data.token,
        });
        return { success: true };
      }
      // Unified error message
      let errorMsg = response.msg || "Login failed";
      if (code === -1 && data && typeof data.retryCount !== "undefined") {
        errorMsg =
          data.retryCount > 0
            ? `${data.retryCount} attempts remaining before temporary lock`
            : "Account locked - Please retry later";
        return { success: false, errorMsg, usePasswordErrorMsg: true };
      }
      return { success: false, errorMsg, usePasswordErrorMsg: false };
    } catch (error) {
      return {
        success: false,
        errorMsg: "Login failed",
        usePasswordErrorMsg: false,
      };
    }
  };

  const onFinish = async (values: { username: string; password: string }) => {
    const userName = values.username;
    const password = values.password;
    const result = await loginAction(userName, password);
    if (!result.success) {
      if (result.usePasswordErrorMsg) {
        setPasswordErrorMsg(result.errorMsg);
      } else {
        setPasswordErrorMsg(null);
        messageApi.error(result.errorMsg);
      }
    } else {
      setPasswordErrorMsg(null);
    }
  };

  return (
    <div className="h-full flex flex-col justify-center items-center text-18">
      <img src={recameraLogo} className="w-300" />
      <div className="text-16 w-500 mt-40 mb-40">
        Welcome! Your exciting journey into the realm of Vision AI Platform
        starts right here. Let reCamera redefine your vision of possibilities.
      </div>
      <Form
        className="w-400"
        name="login"
        labelCol={{ span: 6 }}
        wrapperCol={{ span: 18 }}
        initialValues={{ username: "recamera" }}
        onFinish={onFinish}
      >
        <Form.Item
          name="username"
          label="Username"
          rules={[
            {
              required: true,
              message: "Please input Username",
              whitespace: true,
            },
          ]}
        >
          <Input prefix={<UserOutlined />} placeholder="Username" />
        </Form.Item>
        <Form.Item
          name="password"
          label="Password"
          rules={[requiredTrimValidate()]}
          validateStatus={passwordErrorMsg ? "error" : undefined}
          help={passwordErrorMsg}
          extra={
            !passwordErrorMsg && (
              <>
                * First time login password is&nbsp;
                <span className="font-bold text-primary">"recamera"</span>
              </>
            )
          }
        >
          <Input.Password
            prefix={<LockOutlined />}
            placeholder="Password"
            visibilityToggle
          />
        </Form.Item>
        <Form.Item className="w-full" noStyle>
          <Button block type="primary" htmlType="submit">
            Login
          </Button>
        </Form.Item>
      </Form>

      <Modal
        title="Change password"
        open={firstLogin}
        closable={false}
        footer={
          <Button
            className="w-1/2 m-auto block"
            type="primary"
            loading={loading}
            onClick={handleChangePassword}
          >
            Confirm
          </Button>
        }
      >
        <Form
          form={form}
          name="dependencies"
          autoComplete="off"
          style={{ maxWidth: 600 }}
          layout="vertical"
        >
          <Form.Item
            name="oldpassword"
            label="Old Password"
            rules={[requiredTrimValidate()]}
            extra={
              <>
                For first time login, default password is&nbsp;
                <span className="font-bold text-primary">"recamera"</span>
              </>
            }
          >
            <Input.Password placeholder="recamera" visibilityToggle />
          </Form.Item>
          <Form.Item
            name="newpassword"
            label="New Password"
            rules={passwordRules}
          >
            <Input.Password
              placeholder="Enter new password here"
              visibilityToggle
            />
          </Form.Item>

          <Form.Item
            name="confirmpassword"
            label="Confirm New Password"
            dependencies={["newpassword"]}
            rules={[
              {
                required: true,
              },
              ({ getFieldValue }) => ({
                validator(_, value) {
                  if (!value || getFieldValue("newpassword") === value) {
                    return Promise.resolve();
                  }
                  return Promise.reject(
                    new Error("The new password that you entered do not match!")
                  );
                },
              }),
            ]}
          >
            <Input.Password
              placeholder="Confirm new password"
              visibilityToggle
            />
          </Form.Item>
        </Form>
      </Modal>
      {messageContextHolder}
    </div>
  );
};

export default Login;
