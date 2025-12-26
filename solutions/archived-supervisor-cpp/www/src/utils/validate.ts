// antd-mobile input 空格required验证
export const requiredTrimValidate = () => {
  return {
    required: true,
    validator: (_: unknown, value: string) => {
      if (value && value.trim().length > 0) {
        return Promise.resolve();
      }
      return Promise.reject("Required");
    },
  };
};

export const publicKeyValidate = () => {
  const reg =
    /^(ssh-ed25519|ssh-rsa|ssh-dss)\s+([A-Za-z0-9+/]+[=]{0,2}|(\{?[A-Za-z0-9+/]+?\}?))(\s+.*)?$/;
  return {
    required: true,
    validator: (_: unknown, value: string) => {
      if (value && reg.test(value)) {
        return Promise.resolve();
      }
      return Promise.reject("Invalid Key");
    },
  };
};

// 不能输入中文验证
export const chineseValidate = () => {
  const digitOnly = /^[^一-\u9fa5]+$/; // 匹配任何一个非中文字符
  return {
    required: true,
    validator: (_: unknown, value: string) => {
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
      /^(?=.*[0-9])(?=.*[a-zA-Z])(?=.*[~`!@#$%^&*()_+={}\\|/,.])[0-9a-zA-Z~`!@#$%^&*()_+={}\\|/,.]{8,32}$/,
    message:
      "Password must be 8 to 32 characters and include letters, numbers, and symbols.",
  },
];

// Linux hostname 验证: 小写字母/数字/中划线，不能以中划线开头或结尾，长度 1-32
export const hostnameValidate = (maxLength = 32) => {
  const hostnameReg = new RegExp(
    `^(?=.{1,${maxLength}}$)[a-z0-9](?:[a-z0-9-]*[a-z0-9])?$`
  );
  return {
    required: true,
    validator: (_: unknown, value: string) => {
      const v = (value || "").trim();
      if (!v) return Promise.reject("Required");
      if (!hostnameReg.test(v)) return Promise.reject("Invalid Hostname");
      return Promise.resolve();
    },
  };
};
