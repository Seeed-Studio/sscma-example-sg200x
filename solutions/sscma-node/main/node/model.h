
#pragma once
/**
 * @file model.h
 * @brief ModelNode 类定义
 *
 * 该文件定义了多模型推理节点，支持两种运行模式：
 * 1. 单模型模式：传统的单一模型推理
 * 2. 多模型模式：支持串行和并行两种子模式
 *
 * @note 所有OpenCV类型使用 cv2 命名空间别名
 */

#include <vector>
#include <string>
#include <opencv2/opencv.hpp>

#include "extension/bytetrack/byte_tracker.h"
#include "extension/counter/counter.h"

namespace cv2 = cv;

#include "node.h"
#include "server.h"

#include "camera.h"
#include "porting/ma_osal.h"

namespace ma::node {

/**
 * @struct PreprocessConfig
 * @brief 输入图像预处理配置结构体
 *
 * 用于配置模型输入图像的预处理参数，包括尺寸调整、颜色空间转换、归一化等
 *
 * @note 默认配置：
 *       - 输入尺寸：640x640
 *       - 输入格式：RGB
 *       - 使用letterbox填充
 *       - 启用归一化
 */
struct PreprocessConfig {
    int input_width;         ///< 模型输入宽度
    int input_height;        ///< 模型输入高度
    std::string input_format; ///< 输入格式："RGB"、"BGR"、"GRAY"
    std::vector<float> mean; ///< RGB通道均值，用于归一化
    std::vector<float> std;  ///< RGB通道标准差，用于归一化
    bool swap_rb;            ///< 是否交换R和B通道
    bool normalize;          ///< 是否进行归一化处理
    bool letterbox;          ///< 是否使用letterbox填充方式调整尺寸

    PreprocessConfig() : input_width(640), input_height(640), input_format("RGB"),
                         mean({0.0, 0.0, 0.0}), std({1.0, 1.0, 1.0}),
                         swap_rb(false), normalize(true), letterbox(true) {}
};

class ModelNode : public Node {

public:
    ModelNode(std::string id);
    ~ModelNode();

    ma_err_t onCreate(const json& config) override;
    ma_err_t onStart() override;
    ma_err_t onControl(const std::string& control, const json& data) override;
    ma_err_t onStop() override;
    ma_err_t onDestroy() override;

private:
    /**
     * @class ModelInstance
     * @brief 模型实例类，封装单个模型的所有状态和资源
     *
     * 每个 ModelInstance 代表一个独立运行的模型，包含：
     * - 模型引擎和模型对象指针
     * - 线程管理
     * - 跟踪器和计数器
     * - 预处理配置
     * - 输入输出帧缓冲区（用于串行模式）
     * - 同步机制（互斥锁和事件）
     *
     * @note 在多模型模式下，每个模型实例独立运行，可配置为串行或并行执行
     */
    struct ModelInstance {
        std::string id;                      ///< 模型实例唯一标识
        std::string name;                    ///< 模型名称
        Model* model;                        ///< 模型对象指针
        Engine* engine;                      ///< 推理引擎指针
        Thread* thread;                      ///< 模型推理线程
        BYTETracker tracker;                 ///< 目标跟踪器（用于检测模型）
        Counter counter;                     ///< 计数器（用于计数功能）
        std::vector<std::string> labels;     ///< 类别标签列表
        PreprocessConfig preprocess_config;  ///< 输入预处理配置
        json info;                           ///< 模型配置信息
        bool debug;                          ///< 调试模式开关
        bool trace;                          ///< 轨迹跟踪开关
        bool counting;                       ///< 计数功能开关
        bool websocket;                      ///< WebSocket传输开关
        bool output;                         ///< 输出开关
        TransportWebSocket* transport;       ///< WebSocket传输对象
        int32_t preview_width;   ///< 预览图像宽度
        int32_t preview_height;  ///< 预览图像高度
        int32_t preview_fps;     ///< 预览帧率
        void* node;              ///< 指向所属ModelNode的指针，用于stub函数回调
        cv2::Mat* input_frame;   ///< 串行模式：输入帧数据缓冲区
        cv2::Mat* output_frame;  ///< 串行模式：输出帧数据缓冲区（作为下一个模型的输入）
        bool is_first;           ///< 是否是第一个模型实例（用于串行模式）
        bool is_last;            ///< 是否是最后一个模型实例（用于串行模式）
        Mutex mutex;             ///< 同步机制：互斥锁，保护共享数据访问
        Event event;             ///< 同步机制：事件信号，用于通知数据就绪
        bool data_ready;         ///< 同步机制：数据就绪标志

        /**
         * @brief ModelInstance 构造函数
         * @param model_id 模型实例ID
         * @param model_name 模型名称
         */
        ModelInstance(const std::string& model_id, const std::string& model_name)
            : id(model_id), name(model_name), model(nullptr), engine(nullptr), thread(nullptr),
              debug(false), trace(false), counting(false), websocket(true), output(false),
              transport(nullptr), preview_width(640), preview_height(640), preview_fps(30),
              node(nullptr), input_frame(nullptr), output_frame(nullptr),
              is_first(false), is_last(false), data_ready(false) {}
    };

protected:
    void threadEntry();
    static void threadEntryStub(void* obj);

    /**
     * @brief 模型实例推理线程入口函数
     * @param instance 模型实例指针
     *
     * 该函数为每个模型实例的独立推理线程的执行主体，处理：
     * 1. 帧数据获取（并行模式：独立获取；串行模式：从上一模型获取）
     * 2. 图像预处理
     * 3. 模型推理
     * 4. 结果后处理
     * 5. 结果输出（串行模式：传递给下一个模型）
     *
     * @note 在并行模式下，每个模型独立处理摄像头帧
     * @note 在串行模式下，只有第一个模型从摄像头获取帧，后续模型从共享内存获取
     */
    void modelInstanceThreadEntry(ModelInstance* instance);
    static void modelInstanceThreadEntryStub(void* obj);

    /**
     * @brief 串行执行模式的主线程入口函数
     * @param obj ModelNode指针
     *
     * 串行执行模式的核心逻辑，按顺序依次执行所有模型：
     * 1. 从摄像头获取原始帧
     * 2. 依次调用每个模型的预处理、推理、结果处理
     * 3. 将每个模型的输出作为下一个模型的输入
     * 4. 最终输出最后一个模型的结果
     *
     * @note 适用于需要模型级联处理的场景（如检测+分类+分割）
     */
    void serialExecutionEntry();
    static void serialExecutionEntryStub(void* obj);

    /**
     * @brief 图像预处理函数
     * @param input 输入图像（BGR格式）
     * @param config 预处理配置
     * @return 预处理后的图像（浮点型，32FC3）
     *
     * 预处理流程：
     * 1. 颜色空间转换（BGR -> RGB/GRAY）
     * 2. 尺寸调整（letterbox填充或直接缩放）
     * 3. 数据类型转换（CV_8U -> CV_32F）
     * 4. 归一化（减去均值，除以标准差）
     */
    cv2::Mat preprocessImage(const cv2::Mat& input, const PreprocessConfig& config);

    /**
     * @brief 将原始帧转换为模型输入格式
     * @param original_frame 原始输入帧
     * @param model 模型对象指针
     * @param config 预处理配置
     * @return 符合模型输入要求的图像
     *
     * 与 preprocessImage 的区别：
     * - preprocessImage：通用预处理，用于任意输入
     * - convertResultToInput：专为模型间结果传递设计，处理模型输出的尺寸和格式
     */
    cv2::Mat convertResultToInput(const cv2::Mat& original_frame, Model* model, const PreprocessConfig& config);

    std::string uri_;
    int32_t times_;
    int32_t count_;
    bool debug_;
    bool trace_;
    bool counting_;
    json info_;
    int algorithm_;
    Model* model_;
    Engine* engine_;
    BYTETracker tracker_;
    Counter counter_;
    std::vector<std::string> labels_;
    Thread* thread_;
    CameraNode* camera_;
    MessageBox raw_frame_;
    MessageBox jpeg_frame_;
    bool websocket_;
    bool output_;
    TransportWebSocket* transport_;
    int32_t preview_width_;   ///< 预览图像宽度
    int32_t preview_height_;  ///< 预览图像高度
    int32_t preview_fps_;     ///< 预览帧率

    /**
     * @brief 多模型支持成员变量
     *
     * mode_ 运行模式说明：
     * - "serial"：串行模式，所有模型在单一线程中按顺序执行
     * - "parallel"：并行模式，每个模型在独立线程中并行执行
     *
     * 数据流说明：
     * - 串行模式：ModelInstance.input_frame/output_frame 通过共享内存传递
     * - 并行模式：每个模型独立从摄像头获取帧，无数据传递
     */
    std::vector<ModelInstance*> models_;  ///< 模型实例列表
    std::string mode_;                    ///< 运行模式："serial" 或 "parallel"
    size_t batch_size_;                   ///< 批处理大小
};


}  // namespace ma::node