## 项目框架
react+Typescript+Tailwindcss+vite+antd-mobile+zustand

## 项目启动
- 开发环境运行:
```
npm install
npm run dev
```

- 测试环境打包:
```
npm install
npm run staging
```

- 正式环境打包:
```
npm install
npm run staging
```

## 部署到设备
### 一、将编译生成的dist文件夹传到设备
```
1. sftp recamera@192.168.42.1
  ip地址为设备ip地址，通过usb连接则固定为192.168.42.1

2. put -r dist
  dist为项目编译之后的dist本地目录
```

#### 二、将设备中的dist目录拷贝到www目录
```
1. cd /usr/share/supervisor/

2. sudo rm -rf www/
  如果supervisor目录下面有www目录，则需要先删除

3. sudo cp /home/recamera/dist/ www -r

4. sudo reboot
```

访问 http://192.168.42.1


### 项目目录说明

```
├── App.tsx
├── api // api相关
├── assets
│   ├── images
│   └── style
├── components //全局组件相关
├── enum //全局定义的枚举
├── layout //移动端与pc端布局适配
│   ├── mobile //移动端布局
│   ├── pc //PC端布局
│   └── index //响应式切换移动端PC端布局
├── main.tsx
├── router //路由
├── store // 全局状态管理
│   ├── config //设备配置相关
│   ├── platform //Sensecraft平台相关
│   └── user //设备账号密码相关
├── utils // 工具、方法
├── views // 页面
│   ├── dashboard //nodered的dashboard页面的封装
│   ├── login //Sensecraft平台相关 //设备登录页，与其他路由页面平级
│   ├── files //文件管理相关 //设备文件管理（本地文件和SD卡文件）
│   ├── network //Sensecraft平台相关 //设备网络配置
│   ├── overview //Sensecraft平台相关 //设备预览
│   ├── power //Sensecraft平台相关 //设备开关
│   ├── security //Sensecraft平台相关 //设备安全
│   ├── system //Sensecraft平台相关 //设备系统信息
│   ├── terminal //Sensecraft平台相关 //设备终端
│   └── workspace //Sensecraft平台相关 //Sensecraft工作区
```

## 功能说明

### 文件管理功能
- **路径**: `/files`
- **功能**: 管理设备上的本地文件和SD卡文件
- **特性**:
  - **简化架构**: 移除了复杂的`isRootView`逻辑，采用更直观的storage选择器模式
  - **Storage选择器**: 顶部提供Local Files和SD Card两个按钮，用户可以轻松切换存储设备
  - **统一界面**: 无论选择哪个storage，都使用相同的文件浏览器界面，保持一致性
  - **面包屑导航**: 每个storage都有独立的面包屑路径，显示当前目录层级
  - **完整功能**: 每个storage都支持上传、新建文件夹、重命名、删除、下载等操作
  - **实时显示**: 显示当前目录的文件和文件夹列表，支持排序和搜索
  - **自动检测**: 自动检测SD卡插入状态，动态显示SD Card选项
- **API接口**: 
  - `listFiles(storage, path)` - 获取指定storage和路径的文件列表
  - `checkSdAvailable()` - 检查SD卡可用性
  - `makeDirectory(storage, path)` - 在指定storage创建文件夹
  - `removeEntry(storage, path)` - 删除文件或文件夹
  - `renameEntry(storage, oldPath, newPath)` - 重命名文件或文件夹
  - `downloadAsBlob(storage, path)` - 下载文件
  - `uploadFiles(storage, path, files)` - 上传文件到指定路径

### 技术实现
- 使用React + TypeScript开发
- 采用Ant Design组件库构建UI
- 使用Tailwind CSS进行样式设计
- 支持响应式布局（移动端和PC端）
- 使用Zustand进行状态管理
- Mock数据接口，便于后续接入真实API

### 最近更新 (2024年)
#### 文件管理重构
- **简化架构**: 移除了复杂的根目录视图逻辑，采用更直观的storage选择器
- **统一体验**: 所有storage使用相同的文件浏览器界面，减少代码重复
- **性能优化**: 移除了不必要的数据预加载，按需加载文件列表
- **代码清理**: 删除了未使用的状态变量和复杂的渲染逻辑
- **用户体验**: 顶部storage选择器让用户可以快速切换存储设备，操作更直观
