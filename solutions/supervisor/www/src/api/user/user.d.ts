interface IUserInfo {
  userName: string;
  firstLogin: boolean; //默认true；表示第一次登录，如果是第一次登录，需要修改密码
  sshkeyList: ISshItem[];
}
interface ISshItem {
  id: string;
  name: string;
  value: string;
  addTime: number;
  latestUserdTime: string;
}
