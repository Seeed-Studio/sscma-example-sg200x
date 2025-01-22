import CommonPopup from "@/components/common-popup";
import { Form, Input, Button, TextArea } from "antd-mobile";
import KeyImg from "@/assets/images/svg/key.svg";
import { AddCircleOutline, DeleteOutline } from "antd-mobile-icons";
import { useData, IFormTypeEnum } from "./hook";
import moment from "moment";
import { requiredTrimValidate, passwordRules } from "@/utils/validate";

const publicKeyValidate = () => {
  const reg =
    /^(ssh-ed25519|ssh-rsa|ssh-dss)\s+([A-Za-z0-9+\/]+[=]{0,2}|(\{?[A-Za-z0-9+\/]+?\}?))(\s+.*)?$/;
  return {
    validator: (_: any, value: string) => {
      if (value && reg.test(value)) {
        return Promise.resolve();
      }
      return Promise.reject("Invalid Key");
    },
  };
};
const titleObj = {
  [IFormTypeEnum.Key]: "Add new SSH Key",
  [IFormTypeEnum.Username]: "Edit Username",
  [IFormTypeEnum.Password]: "Confirm Password",
  [IFormTypeEnum.DelKey]: "Remove SSH key",
};
function Security() {
  const {
    state,
    formRef,
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
  } = useData();

  return (
    <div className="my-8 p-16">
      <div className="font-bold text-18 ">User</div>
      <div className="rounded-16 bg-white p-30 mt-14 mb-24">
        <div className="flex justify-between mb-16">
          <span className="text-black opacity-60">Username</span>
          <div className="flex">
            <span>{state.username}</span>
            {/* <img
							className='w-24 h-24 ml-6 self-center'
							onClick={() => onEdit(IFormTypeEnum.Username)}
							src={EditBlackImg}
							alt=''
						/> */}
          </div>
        </div>
        <div className="flex justify-center">
          <Button onClick={() => onEdit(IFormTypeEnum.Password)}>
            <span className="text-3d">Change Password</span>
          </Button>
        </div>
      </div>
      <div className="font-bold text-18 flex justify-between">
        <span>SSH Key</span>
        <AddCircleOutline fontSize={24} color="#8fc31f" onClick={addSshKey} />
      </div>
      <div className="mt-12">
        {state.sshkeyList?.length ? (
          state.sshkeyList.map((item, index) => {
            return (
              <div className="bg-white border rounded-20 p-16 flex mb-16">
                <div className="mr-20">
                  <img className="w-44 h-44 mb-16" src={KeyImg} alt="" />
                  <div className="border px-5 py-3 rounded-2">SSH</div>
                </div>
                <div className="flex-1 truncate">
                  <div className="text-16 flex justify-between">
                    <span className="">{item.name}</span>
                    <DeleteOutline
                      className="font-bold"
                      fontSize={18}
                      color="#D54941"
                      onClick={() => onDelete(item)}
                    />
                  </div>
                  <div className="text-black opacity-60 mt-12 mb-8 text-wrap break-words">
                    {item.value}
                  </div>
                  <div className="text-black opacity-60">
                    <div>
                      Added on
                      {item.addTime &&
                        moment(item.addTime).format(" MMMM DD,YYYY")}
                    </div>
                    {/* <div>
											{item.latestUserdTime
												? `Last used time —— ${moment(item.addTime).format(
														' MMMM DD,YYYY'
												  )}`
												: 'Never Used'}
										</div> */}
                  </div>
                </div>
              </div>
            );
          })
        ) : (
          <div className="text-16 text-3d">
            There are no SSH keys associated with your device.
          </div>
        )}
      </div>
      <CommonPopup
        visible={state.visible}
        title={titleObj[state.formType]}
        onCancel={onCancel}
      >
        {state.formType == IFormTypeEnum.Key && (
          <Form
            ref={formRef}
            className="border-b-0"
            requiredMarkStyle="none"
            onFinish={onAddSshFinish}
            footer={
              <Button block type="submit" color="primary">
                Add SSH Key
              </Button>
            }
          >
            <Form.Item
              name="sshName"
              label="Title"
              rules={[requiredTrimValidate()]}
            >
              <Input placeholder="recamera_132456" clearable maxLength={32} />
            </Form.Item>
            <Form.Item
              name="sshKey"
              label="Key"
              trigger="onChange"
              rules={[publicKeyValidate()]}
            >
              <TextArea
                rows={8}
                placeholder="Begins with 'ssh-rsa', 'ssh-ed25519',  or 'ssh-dss'"
              />
            </Form.Item>
          </Form>
        )}
        {state.formType == IFormTypeEnum.Password && (
          <Form
            ref={passwordFormRef}
            className="border-b-0"
            requiredMarkStyle="none"
            onFinish={onPasswordFinish}
            footer={
              <Button block type="submit" color="primary">
                Confirm
              </Button>
            }
          >
            <Form.Item
              name="oldPassword"
              label="Old Password"
              rules={[requiredTrimValidate()]}
            >
              <Input
                className="border rounded-6 p-10"
                placeholder=""
                clearable
                maxLength={16}
              />
            </Form.Item>
            <Form.Item
              name="newPassword"
              label="New Password"
              rules={passwordRules}
            >
              <Input
                className="border rounded-6 p-10"
                placeholder=""
                clearable
                maxLength={16}
              />
            </Form.Item>
          </Form>
        )}
        {state.formType == IFormTypeEnum.Username && (
          <Form
            ref={usernameFormRef}
            requiredMarkStyle="none"
            onFinish={onUsernameFinish}
            initialValues={{
              username: state.username,
            }}
            footer={
              <Button block type="submit" color="primary">
                Confirm
              </Button>
            }
          >
            <Form.Item
              name="username"
              label="Username"
              rules={[requiredTrimValidate()]}
            >
              <Input placeholder="" clearable maxLength={32} />
            </Form.Item>
          </Form>
        )}
        {state.formType == IFormTypeEnum.DelKey && (
          <div>
            <div className="text-3d text-16 mb-20">
              This action cannot revert, are you sure you want to remove this
              SSH key?
            </div>
            <Button
              block
              shape="rounded"
              type="submit"
              color="danger"
              onClick={onDeleteFinish}
            >
              Confirm
            </Button>
          </div>
        )}
      </CommonPopup>
    </div>
  );
}

export default Security;
