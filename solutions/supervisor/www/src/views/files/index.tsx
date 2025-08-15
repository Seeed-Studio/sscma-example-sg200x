import { useState, useEffect } from "react";
import {
  Button,
  Input,
  Upload,
  Modal,
  message,
  Spin,
  Breadcrumb,
  Dropdown,
  Empty,
  Radio,
  Progress,
} from "antd";
import type {
  BreadcrumbProps,
  MenuProps,
  DropdownProps,
  RadioChangeEvent,
} from "antd";
import type { UploadFile, RcFile } from "antd/es/upload/interface";
import {
  FolderFilled,
  FileOutlined,
  VideoCameraOutlined,
  PictureOutlined,
  ExperimentOutlined,
  UploadOutlined,
  PlusOutlined,
  DeleteOutlined,
  EditOutlined,
  DownloadOutlined,
} from "@ant-design/icons";
import {
  listFiles,
  checkSdAvailable,
  makeDirectory,
  removeEntry,
  renameEntry,
  downloadAsBlob,
  uploadFiles,
  StorageType,
  FileListData,
  DirectoryEntry,
  FileEntry,
} from "@/api/files";

type SortField = "name" | "size" | "type" | "modified";
type SortDirection = "asc" | "desc";

interface SortConfig {
  field: SortField;
  direction: SortDirection;
}

const PROTECTED_ROOT_DIRS = ["Models", "Videos", "Images"];

const normalizePath = (path: string) => path.replace(/^\/+/, "");
const isProtectedRootDir = (name: string) =>
  PROTECTED_ROOT_DIRS.includes(normalizePath(name));

const Files = () => {
  // 状态管理
  const [currentStorage, setCurrentStorage] = useState<StorageType>("local");
  const [currentPath, setCurrentPath] = useState<string>("");
  const [fileListData, setFileListData] = useState<FileListData | null>(null);
  const [loading, setLoading] = useState(false);
  const [sdCardAvailable, setSdCardAvailable] = useState(false);

  // 文件操作状态
  const [selectedFile, setSelectedFile] = useState<string | null>(null);
  const sortConfig: SortConfig = {
    field: "name",
    direction: "asc",
  };
  const [uploadModalVisible, setUploadModalVisible] = useState(false);
  const [newFolderModalVisible, setNewFolderModalVisible] = useState(false);
  const [renameModalVisible, setRenameModalVisible] = useState(false);
  const [newFolderName, setNewFolderName] = useState("");
  const [newFileName, setNewFileName] = useState("");
  const [uploadFileList, setUploadFileList] = useState<UploadFile[]>([]);

  // 上传进度状态
  const [uploadProgress, setUploadProgress] = useState<{
    visible: boolean;
    progress: number;
  }>({
    visible: false,
    progress: 0,
  });

  // 图片预览状态
  const [previewLoading, setPreviewLoading] = useState(false);
  const [previewModalVisible, setPreviewModalVisible] = useState(false);
  const [previewImageUrl, setPreviewImageUrl] = useState("");
  const [previewFileName, setPreviewFileName] = useState("");

  // 图片缓存 - 使用Map存储已下载的图片blob URL
  const [imageCache, setImageCache] = useState<Map<string, string>>(new Map());

  // 导航历史
  const [navigationHistory, setNavigationHistory] = useState<
    Array<{ storage: StorageType; path: string }>
  >([]);
  const [historyIndex, setHistoryIndex] = useState(-1);

  // 初始化
  useEffect(() => {
    checkSdCardAvailability();
    // 默认加载local storage的根目录
    loadFileList("local", "");
  }, []);

  // 检查 SD 卡可用性
  const checkSdCardAvailability = async () => {
    try {
      const available = await checkSdAvailable();
      setSdCardAvailable(available);
    } catch (error) {
      console.error("Failed to check SD card availability:", error);
      setSdCardAvailable(false);
    }
  };

  // 消息实例
  const [messageApi, messageContextHolder] = message.useMessage();

  // 加载文件列表
  const loadFileList = async (storage: StorageType, path: string = "") => {
    setLoading(true);
    try {
      const data = await listFiles(storage, path);
      setFileListData(data);
      setCurrentStorage(storage);
      setCurrentPath(path);

      // 更新导航历史
      const newHistory = [
        ...navigationHistory.slice(0, historyIndex + 1),
        { storage, path },
      ];
      setNavigationHistory(newHistory);
      setHistoryIndex(newHistory.length - 1);
    } catch (error) {
      console.error("Failed to load file list:", error);
      messageApi.error("Failed to load file list");
    } finally {
      setLoading(false);
    }
  };

  // 导航到目录
  const navigateToDirectory = (dirName: string) => {
    const newPath = currentPath ? `${currentPath}/${dirName}` : dirName;
    loadFileList(currentStorage, newPath);
  };

  // 获取排序后的文件列表
  const getSortedItems = () => {
    if (!fileListData) return { directories: [], files: [] };

    const { directories, files } = fileListData;
    const allItems = [
      ...directories.map((dir) => ({ ...dir, type: "directory" as const })),
      ...files.map((file) => ({ ...file, type: "file" as const })),
    ];

    allItems.sort((a, b) => {
      const multiplier = sortConfig.direction === "asc" ? 1 : -1;
      switch (sortConfig.field) {
        case "name":
          return multiplier * a.name.localeCompare(b.name);
        case "size":
          return multiplier * ((a.size || 0) - (b.size || 0));
        case "type":
          return multiplier * a.type.localeCompare(b.type);
        case "modified":
          return multiplier * ((a.modified || 0) - (b.modified || 0));
        default:
          return 0;
      }
    });

    return {
      directories: allItems.filter((item) => item.type === "directory"),
      files: allItems.filter((item) => item.type === "file"),
    };
  };

  // 获取文件图标（支持自定义尺寸）
  const getFileIcon = (
    filename: string,
    isDirectory: boolean,
    size: number = 42
  ) => {
    if (isDirectory) {
      return <FolderFilled style={{ color: "#C8DF92", fontSize: 56 }} />;
    }
    const ext = filename.split(".").pop()?.toLowerCase();
    switch (ext) {
      case "mp4":
      case "avi":
      case "mov":
        return (
          <VideoCameraOutlined style={{ color: "#8c8c8c", fontSize: size }} />
        );
      case "jpg":
      case "jpeg":
      case "png":
      case "gif":
        return <PictureOutlined style={{ color: "#8c8c8c", fontSize: size }} />;
      case "onnx":
      case "pth":
      case "pt":
        return (
          <ExperimentOutlined style={{ color: "#8c8c8c", fontSize: size }} />
        );
      default:
        return <FileOutlined style={{ color: "#8c8c8c", fontSize: size }} />;
    }
  };

  // 通用磁贴渲染
  type TileOptions = {
    key: string;
    name: string;
    isDirectory: boolean;
    selected: boolean;
    onClick: () => void;
    onDoubleClick: () => void;
    contextMenu?: NonNullable<DropdownProps["menu"]>;
  };

  const renderTile = ({
    key,
    name,
    isDirectory,
    selected,
    onClick,
    onDoubleClick,
    contextMenu,
  }: TileOptions) => {
    const content = (
      <div
        className="flex flex-col items-center cursor-pointer group my-4"
        onClick={onClick}
        onDoubleClick={onDoubleClick}
        title={name}
      >
        <div
          className={`w-[100px] h-[70px] flex items-center justify-center rounded mb-4 ${
            selected ? "bg-gray-200" : ""
          }`}
        >
          {getFileIcon(name, isDirectory)}
        </div>
        <div
          className={`text-sm truncate px-1 max-w-[100px] ${
            selected ? "bg-primary text-white rounded" : "text-gray-700"
          }`}
        >
          {name}
        </div>
      </div>
    );
    if (contextMenu) {
      return (
        <Dropdown key={key} menu={contextMenu} trigger={["contextMenu"]}>
          {content}
        </Dropdown>
      );
    }
    return <div key={key}>{content}</div>;
  };

  // 检查是否为图片文件
  const isImageFile = (filename: string): boolean => {
    const ext = filename.split(".").pop()?.toLowerCase();
    return ["jpg", "jpeg", "png", "gif", "bmp", "webp"].includes(ext || "");
  };

  // 生成缓存key
  const getCacheKey = (storage: StorageType, path: string): string => {
    return `${storage}:${path}`;
  };

  // 预览图片
  const handleImagePreview = async (filename: string) => {
    if (!isImageFile(filename)) {
      messageApi.warning("This file is not an image");
      return;
    }

    // 如果正在加载中，直接返回
    if (previewLoading) {
      return;
    }

    const fullPath = currentPath ? `${currentPath}/${filename}` : filename;
    const cacheKey = getCacheKey(currentStorage, fullPath);

    // 检查缓存
    const cachedUrl = imageCache.get(cacheKey);
    if (cachedUrl) {
      // 如果缓存存在，直接使用
      setPreviewImageUrl(cachedUrl);
      setPreviewFileName(filename);
      setPreviewModalVisible(true);
      return;
    }

    try {
      // 先显示弹窗和loading状态
      setPreviewModalVisible(true);
      setPreviewLoading(true);
      setPreviewImageUrl("");
      setPreviewFileName(filename);

      // 使用下载接口获取图片数据
      const blob = await downloadAsBlob(currentStorage, fullPath);

      // 创建blob URL用于预览
      const imageUrl = URL.createObjectURL(blob);

      // 缓存图片URL
      setImageCache((prev) => new Map(prev).set(cacheKey, imageUrl));

      setPreviewImageUrl(imageUrl);
    } catch (error) {
      console.error("Failed to preview image:", error);
      messageApi.error("Failed to load image for preview");
      // 加载失败时关闭弹窗
      setPreviewModalVisible(false);
    } finally {
      setPreviewLoading(false);
    }
  };

  // 关闭图片预览
  const handleCloseImagePreview = () => {
    setPreviewModalVisible(false);
    setPreviewImageUrl("");
    setPreviewFileName("");
  };

  // 清理指定storage的缓存
  const clearStorageCache = (storage: StorageType) => {
    setImageCache((prev) => {
      const newCache = new Map(prev);
      // 清理指定storage的所有缓存
      for (const [key] of newCache) {
        if (key.startsWith(`${storage}:`)) {
          const url = newCache.get(key);
          if (url) {
            URL.revokeObjectURL(url);
          }
          newCache.delete(key);
        }
      }
      return newCache;
    });
  };

  // 组件卸载时清理所有缓存
  useEffect(() => {
    return () => {
      // 清理所有缓存的blob URL
      imageCache.forEach((url) => {
        URL.revokeObjectURL(url);
      });
    };
  }, []);

  // 切换storage时清理缓存
  useEffect(() => {
    // 当storage改变时，清理其他storage的缓存以节省内存
    const otherStorage = currentStorage === "local" ? "sd" : "local";
    clearStorageCache(otherStorage);
  }, [currentStorage]);

  // 构建文件浏览器面包屑
  const buildBrowserBreadcrumbItems = (): NonNullable<
    BreadcrumbProps["items"]
  > => {
    const pathParts = currentPath ? currentPath.split("/") : [];
    const rootTitle = currentStorage === "local" ? "/userdata" : "/mnt/sd";
    const items: NonNullable<BreadcrumbProps["items"]> = [
      {
        title: rootTitle,
        href: "#",
        onClick: (e: React.MouseEvent) => {
          e.preventDefault();
          // 返回storage根目录并刷新数据
          setCurrentPath("");
          setSelectedFile(null);
          loadFileList(currentStorage, "");
        },
      },
      ...pathParts.map((part, index) => {
        const isLast = index === pathParts.length - 1;
        if (isLast) return { title: part };
        const newPath = pathParts.slice(0, index + 1).join("/");
        return {
          title: part,
          href: "#",
          onClick: (e: React.MouseEvent) => {
            e.preventDefault();
            loadFileList(currentStorage, newPath);
          },
        };
      }),
    ];
    return items;
  };

  // 新建文件夹
  const handleCreateFolder = async () => {
    if (!newFolderName.trim()) {
      messageApi.error("Please enter a folder name");
      return;
    }

    try {
      const fullPath = currentPath
        ? `${currentPath}/${newFolderName}`
        : newFolderName;
      await makeDirectory(currentStorage, fullPath);
      messageApi.success("Folder created successfully");
      setNewFolderModalVisible(false);
      setNewFolderName("");
      loadFileList(currentStorage, currentPath);
    } catch (error) {
      console.error("Failed to create folder:", error);
      messageApi.error("Failed to create folder");
    }
  };

  // 上传文件
  const handleUpload = async () => {
    if (uploadFileList.length === 0) {
      messageApi.error("Please select files to upload");
      return;
    }

    try {
      // 将 uploadFileList 转换为 FileList
      const filesArray = uploadFileList
        .map((f) => f.originFileObj)
        .filter((f): f is RcFile => Boolean(f));

      const dataTransfer = new DataTransfer();
      filesArray.forEach((file) => dataTransfer.items.add(file));
      const fileList = dataTransfer.files;

      // 显示上传进度
      setUploadProgress({
        visible: true,
        progress: 0,
      });

      // 统一使用分片上传
      await uploadFiles(
        currentStorage,
        currentPath,
        fileList,
        (totalProgress) => {
          setUploadProgress((prev) => ({
            ...prev,
            progress: totalProgress,
          }));
        }
      );

      // 隐藏进度弹窗
      setUploadProgress({ visible: false, progress: 0 });
      messageApi.success("Files uploaded successfully");
      setUploadModalVisible(false);
      setUploadFileList([]);
      loadFileList(currentStorage, currentPath);
    } catch (error) {
      console.error("Failed to upload files:", error);
      messageApi.error("Failed to upload files");
      // 隐藏进度弹窗
      setUploadProgress({ visible: false, progress: 0 });
    }
  };

  // 删除文件/文件夹
  const handleDelete = async (name: string, isDirectory: boolean) => {
    // 保护：根目录的受保护文件夹本身不能被删除
    if (currentPath === "" && isDirectory && isProtectedRootDir(name)) {
      messageApi.warning("This directory cannot be deleted");
      return;
    }

    Modal.confirm({
      title: `Delete ${isDirectory ? "Folder" : "File"}`,
      content: `Are you sure you want to delete "${name}"? This action cannot be undone.`,
      okText: "Delete",
      okType: "danger",
      cancelText: "Cancel",
      onOk: async () => {
        try {
          const fullPath = currentPath ? `${currentPath}/${name}` : name;
          await removeEntry(currentStorage, fullPath);
          messageApi.success("Deleted successfully");
          loadFileList(currentStorage, currentPath);
        } catch (error) {
          console.error("Failed to delete:", error);
          messageApi.error("Failed to delete");
        }
      },
    });
  };

  // 重命名文件/文件夹
  const handleRename = async () => {
    if (!selectedFile || !newFileName.trim()) {
      messageApi.error("Please enter a new name");
      return;
    }

    // 保护：根目录的受保护文件夹本身不能被重命名
    if (currentPath === "" && isProtectedRootDir(selectedFile)) {
      messageApi.warning("This directory cannot be renamed");
      return;
    }

    try {
      const oldPath = currentPath
        ? `${currentPath}/${selectedFile}`
        : selectedFile;
      const newPath = currentPath
        ? `${currentPath}/${newFileName}`
        : newFileName;
      await renameEntry(currentStorage, oldPath, newPath);
      messageApi.success("Renamed successfully");
      setRenameModalVisible(false);
      setNewFileName("");
      setSelectedFile(null);
      loadFileList(currentStorage, currentPath);
    } catch (error) {
      console.error("Failed to rename:", error);
      messageApi.error("Failed to rename");
    }
  };

  // 下载文件
  const handleDownload = async (name: string) => {
    try {
      setLoading(true);
      const fullPath = currentPath ? `${currentPath}/${name}` : name;
      const blob = await downloadAsBlob(currentStorage, fullPath);

      const url = URL.createObjectURL(blob);
      const link = document.createElement("a");
      link.href = url;
      link.download = name;
      document.body.appendChild(link);
      link.click();
      document.body.removeChild(link);
      URL.revokeObjectURL(url);

      messageApi.success("Downloaded successfully");
    } catch (error) {
      console.error("Failed to download:", error);
      messageApi.error("Failed to download");
    } finally {
      setLoading(false);
    }
  };

  // 文件操作菜单（Dropdown.menu）
  const buildFileMenu = (
    item: DirectoryEntry | FileEntry,
    isDirectory: boolean
  ): NonNullable<DropdownProps["menu"]> => {
    // 保护：根目录的受保护文件夹本身不能被删除或重命名
    const disableModify =
      currentPath === "" && isDirectory && isProtectedRootDir(item.name);

    const items: MenuProps["items"] = [
      ...(isDirectory
        ? [{ key: "open", icon: <FolderFilled />, label: "Open" }]
        : [
            ...(isImageFile(item.name)
              ? [
                  {
                    key: "preview",
                    icon: previewLoading ? (
                      <Spin size="small" />
                    ) : (
                      <PictureOutlined />
                    ),
                    label: previewLoading ? "Loading..." : "Preview",
                    disabled: previewLoading,
                  },
                ]
              : []),
            { key: "download", icon: <DownloadOutlined />, label: "Download" },
          ]),
      {
        key: "rename",
        icon: <EditOutlined />,
        label: "Rename",
        disabled: disableModify,
      },
      {
        key: "delete",
        icon: <DeleteOutlined />,
        label: "Delete",
        danger: true,
        disabled: disableModify,
      },
    ];

    return {
      items,
      onClick: ({ key }) => {
        if (key === "open") {
          navigateToDirectory(item.name);
          return;
        }
        if (key === "preview") {
          if (previewLoading) return; // 防止重复点击
          handleImagePreview(item.name);
          return;
        }
        if (key === "download") {
          handleDownload(item.name);
          return;
        }
        if (key === "rename") {
          if (disableModify) return;
          setSelectedFile(item.name);
          setNewFileName(item.name);
          setRenameModalVisible(true);
          return;
        }
        if (key === "delete") {
          if (disableModify) return;
          handleDelete(item.name, isDirectory);
        }
      },
    };
  };

  // 渲染文件浏览器界面
  const renderFileBrowser = () => {
    const { directories, files } = getSortedItems();

    const renderDirTile = (dir: DirectoryEntry) => {
      const selected = selectedFile === dir.name;
      return renderTile({
        key: `dir-${dir.name}`,
        name: dir.name,
        isDirectory: true,
        selected,
        onClick: () => setSelectedFile(dir.name),
        onDoubleClick: () => navigateToDirectory(dir.name),
        contextMenu: buildFileMenu(dir, true),
      });
    };

    const renderFileTile = (file: FileEntry) => {
      const selected = selectedFile === file.name;
      const isImage = isImageFile(file.name);

      return renderTile({
        key: `file-${file.name}`,
        name: file.name,
        isDirectory: false,
        selected,
        onClick: () => setSelectedFile(file.name),
        onDoubleClick: () => {
          if (isImage) {
            if (previewLoading) return; // 防止重复点击
            handleImagePreview(file.name);
          } else {
            handleDownload(file.name);
          }
        },
        contextMenu: buildFileMenu(file, false),
      });
    };

    return (
      <div className="h-full flex flex-col">
        {/* 顶部区域：面包屑与操作按钮 */}
        <div className="bg-white rounded-lg p-8">
          <div className="flex items-center justify-between">
            <Breadcrumb items={buildBrowserBreadcrumbItems()} />
            <div className="flex items-center gap-2">
              <Button
                icon={<UploadOutlined />}
                onClick={() => setUploadModalVisible(true)}
                type="primary"
              >
                {/* Upload Files */}
              </Button>
              <Button
                icon={<PlusOutlined />}
                onClick={() => setNewFolderModalVisible(true)}
              >
                {/* New Folder */}
              </Button>
            </div>
          </div>
        </div>

        {/* 文件列表 */}
        <div className="flex-1 overflow-auto">
          {loading ? (
            <div className="flex justify-center items-center h-full">
              <Spin size="large" />
            </div>
          ) : directories.length === 0 && files.length === 0 ? (
            <div className="flex justify-center items-center h-full">
              <Empty description="This folder is empty" />
            </div>
          ) : (
            <div className="px-4 pb-4">
              <div className="flex flex-wrap gap-6 md:gap-8 lg:gap-10">
                {directories.map((d: DirectoryEntry) => renderDirTile(d))}
                {files.map((f: FileEntry) => renderFileTile(f))}
              </div>
            </div>
          )}
        </div>

        {/* Status bar */}
        <div className="bg-gray-50 border-t border-gray-200 px-4 py-2 text-sm text-gray-600">
          <div className="flex justify-between items-center">
            <span>
              {directories.length + files.length} items
              {directories.length > 0 && ` (${directories.length} folders)`}
              {files.length > 0 && ` (${files.length} files)`}
            </span>
          </div>
        </div>
      </div>
    );
  };

  // 渲染storage选择器
  const renderStorageSelector = () => {
    const handleStorageChange = (e: RadioChangeEvent) => {
      const newStorage = e.target.value as StorageType;
      setCurrentStorage(newStorage);
      setCurrentPath("");
      setSelectedFile(null);
      loadFileList(newStorage, "");
    };

    const radioOptions = [
      { label: "Local Files", value: "local" },
      ...(sdCardAvailable ? [{ label: "SD Card", value: "sd" }] : []),
    ];

    return (
      <div className="p-6">
        <div className="flex items-center">
          {sdCardAvailable ? (
            <Radio.Group
              value={currentStorage}
              onChange={handleStorageChange}
              optionType="button"
              buttonStyle="solid"
              options={radioOptions}
            />
          ) : (
            <div className="text-2xl font-bold text-gray-900">Local Files</div>
          )}
        </div>
      </div>
    );
  };

  return (
    <div className="h-full mt-24">
      {messageContextHolder}

      {/* Storage选择器 */}
      {renderStorageSelector()}

      {/* 文件浏览器 */}
      {renderFileBrowser()}

      {/* Image Preview Modal */}
      <Modal
        title={previewFileName || "Image Preview"}
        open={previewModalVisible}
        onCancel={handleCloseImagePreview}
        footer={null}
        centered
      >
        {previewLoading ? (
          <div className="flex flex-col justify-center items-center h-64">
            <Spin size="large" />
            <div className="mt-4 text-gray-500">Loading image...</div>
            <div className="text-sm text-gray-400 mt-2">
              This may take a moment for large images
            </div>
          </div>
        ) : (
          <div className="flex justify-center">
            <img
              src={previewImageUrl}
              alt={previewFileName}
              style={{
                maxWidth: "100%",
                maxHeight: "70vh",
                objectFit: "contain",
              }}
              onError={(e) => {
                console.error("Image load error:", e);
                messageApi.error("Failed to display image");
              }}
            />
          </div>
        )}
      </Modal>

      {/* New Folder Modal */}
      <Modal
        title="New Folder"
        open={newFolderModalVisible}
        onOk={handleCreateFolder}
        onCancel={() => {
          setNewFolderModalVisible(false);
          setNewFolderName("");
        }}
        okText="Create"
        cancelText="Cancel"
      >
        <Input
          placeholder="Enter folder name"
          value={newFolderName}
          onChange={(e) => setNewFolderName(e.target.value)}
          onPressEnter={handleCreateFolder}
        />
      </Modal>

      {/* Upload Files Modal */}
      <Modal
        title="Upload Files"
        open={uploadModalVisible}
        onOk={handleUpload}
        onCancel={() => {
          setUploadModalVisible(false);
          setUploadFileList([]);
        }}
        okText="Upload"
        cancelText="Cancel"
        confirmLoading={loading}
      >
        <Upload
          fileList={uploadFileList}
          beforeUpload={() => false}
          onChange={({ fileList }) => setUploadFileList(fileList)}
          multiple
        >
          <Button icon={<UploadOutlined />}>Select files</Button>
        </Upload>
      </Modal>

      {/* Rename Modal */}
      <Modal
        title="Rename"
        open={renameModalVisible}
        onOk={handleRename}
        onCancel={() => {
          setRenameModalVisible(false);
          setNewFileName("");
          setSelectedFile(null);
        }}
        okText="Rename"
        cancelText="Cancel"
      >
        <Input
          placeholder="Enter a new name"
          value={newFileName}
          onChange={(e) => setNewFileName(e.target.value)}
          onPressEnter={handleRename}
        />
      </Modal>

      {/* Upload Progress Modal */}
      <Modal
        title="Upload Progress"
        open={uploadProgress.visible}
        footer={null}
        closable={false}
        centered
        width={400}
      >
        <div className="space-y-4">
          <div>
            <Progress
              percent={Math.round(uploadProgress.progress)}
              status="active"
              strokeColor="#1890ff"
            />
          </div>

          <div className="text-center text-sm text-gray-500">
            Uploading files, please wait...
          </div>
        </div>
      </Modal>
    </div>
  );
};

export default Files;
