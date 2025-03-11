import { LockOutlined, UserOutlined } from "@ant-design/icons";
import { Button, Form, Input, Modal, message } from "antd";
import recameraLogo from "@/assets/images/recamera.png";
import useUserStore from "@/store/user";
import { encryptPassword } from "@/utils";
import { requiredTrimValidate, passwordRules } from "@/utils/validate";
import { loginApi, updateUserPasswordApi } from "@/api/user";

const Login = () => {
  const { firstLogin, updateFirstLogin, updateUserInfo } = useUserStore();

  const [form] = Form.useForm();
  const [messageApi, messageContextHolder] = message.useMessage();

  const handleChangePassword = async () => {
    const fieldsValue = form.getFieldsValue();
    const oldpassword = fieldsValue.oldpassword;
    const newpassword = fieldsValue.newpassword;
    const confirmpassword = fieldsValue.confirmpassword;
    if (
      oldpassword &&
      newpassword &&
      confirmpassword &&
      newpassword == confirmpassword
    ) {
      const encryptedOldpassword = encryptPassword(oldpassword);
      const encryptedNewPassword = encryptPassword(newpassword);
      const response = await updateUserPasswordApi({
        oldPassword: encryptedOldpassword,
        newPassword: encryptedNewPassword,
      });
      if (response.code == 0) {
        messageApi.success("Password changed successfully");
        updateFirstLogin(false);
      } else {
        messageApi.error("Password changed failed");
      }
    }
  };

  const loginAction = async (userName: string, password: string) => {
    try {
      const encryptedPassword = encryptPassword(password);
      const response = await loginApi({
        userName,
        password: encryptedPassword,
      });
      if (response.code == 0) {
        const data = response.data;
        updateUserInfo({
          userName,
          password: encryptedPassword,
          token: data.token,
        });
        return true;
      }
      return false;
    } catch (error) {
      return false;
    }
  };

  const onFinish = async (values: { username: string; password: string }) => {
    const userName = values.username;
    const password = values.password;

    const ret = await loginAction(userName, password);
    if (!ret) {
      messageApi.error("Login failed");
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
          extra={
            <>
              * First time login password is{" "}
              <span className="font-bold text-primary">"recamera"</span>
            </>
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
