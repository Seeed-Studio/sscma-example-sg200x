interface IUserInfo {
  userName: string;
  firstLogin: boolean; // Default true; indicates first login, if first login, user needs to change password
  sshkeyList: ISshItem[];
  sshEnabled: boolean; // Whether SSH is enabled, default false
}
interface ISshItem {
  id: string;
  name: string;
  value: string;
  addTime: number;
  latestUserdTime: string;
}
