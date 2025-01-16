// antd-mobile input 空格required验证
export const requiredTrimValidate = () => {
  return {
    required: true,
    validator: (_: any, value: string) => {
      if (value && value.trim().length > 0) {
        return Promise.resolve();
      }
      return Promise.reject("Required");
    },
  };
};
// 不能输入中文验证
export const chineseValidate = () => {
  const digitOnly = /^[^一-\u9fa5]+$/; // 匹配任何一个非中文字符
  return {
    required: true,
    validator: (_: any, value: string) => {
      if (value && digitOnly.test(value)) {
        return Promise.resolve();
      }
      return Promise.reject("Cannot enter Chinese");
    },
  };
};

export const passwordRules = [
  { required: true, message: "Please input Password" },
  {
    pattern:
      /^(?=.*[0-9])(?=.*[a-zA-Z])(?=.*[~`!@#$%^&*()_+={}[\]|\\\/,.])[0-9a-zA-Z~`!@#$%^&*()_+={}[\]|\\\/,.]{8,32}$/,
    message:
      "Password must be 8 to 32 characters and include letters, numbers, and symbols.",
  },
];