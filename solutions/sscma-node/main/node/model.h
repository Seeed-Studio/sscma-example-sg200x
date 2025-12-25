
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

    static void frameDispatchEntryStub(void* obj);
    void frameDispatchEntry();

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
     * @section 多模型架构说明
     *
     * 本节点支持三种运行模式，通过 mode_ 和 parallel_mode_ 参数控制：
     *
     * ┌─────────────────────────────────────────────────────────────────────────────┐
     * │                           多模型运行模式架构                                  │
     * ├─────────────────────────────────────────────────────────────────────────────┤
     │                                                                              │
     │  ┌──────────────────────────────────────────────────────────────────────┐    │
     │  │                    模式 1: 串行模式 (mode_="serial")                  │    │
     │  │                                                                      │    │
     │  │   摄像头 ──► Model1 ──► Model2 ──► Model3 ──► 输出                    │    │
     │  │            │         │         │                                       │    │
     │  │            └────┬────┴────┬────┘                                       │    │
     │  │                 │         │                                            │    │
     │  │                 ▼         ▼                                            │    │
     │  │            input_frame  output_frame (共享内存)                         │    │
     │  │                                                                      │    │
     │  │   特点：单线程顺序执行，保证数据流顺序一致性                              │    │
     │  └──────────────────────────────────────────────────────────────────────┘    │
     │                                                                              │
     │  ┌──────────────────────────────────────────────────────────────────────┐    │
     │  │               模式 2a: 并行独立模式 (parallel_mode_="independent")    │    │
     │  │                                                                      │    │
     │  │   ┌──────────┐  ┌──────────┐  ┌──────────┐                           │    │
     │  │   │ 摄像头   │  │ Model1   │  │  输出1   │                           │    │
     │  │   │   获取   │──► 独立    │──► WebSocket                             │    │
     │  │   │   帧1    │  │ 处理     │                                         │    │
     │  │   └──────────┘  └──────────┘                                         │    │
     │  │        │                                                           │    │
     │  │        │  ┌──────────┐  ┌──────────┐                               │    │
     │  │        └──► Model2   │  │  输出2   │                               │    │
     │  │           │ 独立    │──► WebSocket                                 │    │
     │  │           │ 处理    │                                             │    │
     │  │           └──────────┘                                            │    │
     │  │        │                                                         │    │
     │  │        │  ┌──────────┐  ┌──────────┐                               │    │
     │  │        └──► Model3   │  │  输出3   │                               │    │
     │  │           │ 独立    │──► WebSocket                                 │    │
     │  │           │ 处理    │                                             │    │
     │  │           └──────────┘                                            │    │
     │  │                                                                      │    │
     │  │   特点：每个模型独立获取帧，处理速度快的模型处理更多帧                   │    │
     │  │   适用：模型处理速度差异大，需要最大化吞吐量的场景                      │    │
     │  └──────────────────────────────────────────────────────────────────────┘    │
     │                                                                              │
     │  ┌──────────────────────────────────────────────────────────────────────┐    │
     │  │                模式 2b: 并行同步模式 (parallel_mode_="sync")         │    │
     │  │                                                                      │    │
     │  │                    ┌─────────────┐                                   │    │
     │  │   摄像头 ─────────►│ 帧分发线程  │───────────┐                       │    │
     │  │                    │ (主线程)    │           │                       │    │
     │  │                    └─────────────┘           │                       │    │
     │  │                         │                    │                       │    │
     │  │                         ▼                    ▼                       │    │
     │  │              ┌─────────────────┐  ┌─────────────────┐               │    │
     │  │              │ frame_ready_    │  │ frame_ready_    │               │    │
     │  │              │    event        │  │    event        │               │    │
     │  │              └────────┬────────┘  └────────┬────────┘               │    │
     │  │                       │                   │                         │    │
     │  │                       ▼                   ▼                         │    │
     │  │              ┌─────────────┐  ┌─────────────┐                       │    │
     │  │              │  Model1     │  │  Model2     │                       │    │
     │  │              │ 线程1       │  │ 线程2       │                       │    │
     │  │              │ 处理帧N     │  │ 处理帧N     │                       │    │
     │  │              └──────┬──────┘  └──────┬──────┘                       │    │
     │  │                     │               │                               │    │
     │  │                     ▼               ▼                               │    │
     │  │              ┌─────────────────────────────────┐                   │    │
     │  │              │    sync_mutex_ 保护共享数据      │                   │    │
     │  │              │    completed_count_ 计数        │                   │    │
     │  │              │    all_done_event_ 等待完成      │                   │    │
     │  │              └─────────────────────────────────┘                   │    │
     │  │                         │                                            │    │
     │  │                         ▼                                            │    │
     │  │              ┌─────────────────┐                                     │    │
     │  │              │  释放帧N，     │                                     │    │
     │  │              │  获取帧N+1     │                                     │    │
     │  │              └─────────────────┘                                     │    │
     │  │                                                                      │    │
     │  │   特点：所有模型同时处理同一帧，保证时间戳一致性                        │    │
     │  │   适用：多模型结果需要时间对齐的场景（如多任务学习）                    │    │
     │  └──────────────────────────────────────────────────────────────────────┘    │
     │                                                                              │
     * └─────────────────────────────────────────────────────────────────────────────┘
     *
     * @subsection 并行模式对比表
     *
     * ┌─────────────────┬────────────────────────┬────────────────────────┐
     * │     特性        │   independent 模式      │     sync 模式          │
     * ├─────────────────┼────────────────────────┼────────────────────────┤
     * │ 帧获取方式      │ 每个模型独立获取        │ 主线程统一分发          │
     * ├─────────────────┼────────────────────────┼────────────────────────┤
     * │ 处理顺序        │ 无序（各自速度）        │ 严格同步                │
     * ├─────────────────┼────────────────────────┼────────────────────────┤
     * │ 时间戳一致性    │ 不一致                  │ 一致                    │
     * ├─────────────────┼────────────────────────┼────────────────────────┤
     * │ 吞吐量          │ 高（快的模型处理更多）  │ 受限于最慢模型          │
     * ├─────────────────┼────────────────────────┼────────────────────────┤
     * │ 适用场景        │ 独立任务，最大吞吐量    │ 多任务，结果需对齐      │
     * ├─────────────────┼────────────────────────┼────────────────────────┤
     * │ 同步开销        │ 无                     │ 需要事件同步            │
     * └─────────────────┴────────────────────────┴────────────────────────┘
     *
     * @subsection 串行与并行模式对比
     *
     * ┌─────────────────┬────────────────────────┬────────────────────────┐
     * │     特性        │   serial 模式          │   parallel 模式        │
     * ├─────────────────┼────────────────────────┼────────────────────────┤
     * │ 线程数量        │ 1个（单线程）          │ N+1个（N个模型+分发）   │
     * ├─────────────────┼────────────────────────┼────────────────────────┤
     * │ 数据传递        │ input/output_frame     │ 独立/共享帧             │
     * ├─────────────────┼────────────────────────┼────────────────────────┤
     * │ 同步机制        │ mutex + event          │ sync_mutex + events    │
     * ├─────────────────┼────────────────────────┼────────────────────────┤
     * │ 实现复杂度      │ 较低                   │ 较高                    │
     * ├─────────────────┼────────────────────────┼────────────────────────┤
     * │ 适用场景        │ 模型级联处理           │ 多模型独立/同步推理     │
     * └─────────────────┴────────────────────────┴────────────────────────┘
     */
    std::vector<ModelInstance*> models_;  ///< 模型实例列表
    std::string mode_;                    ///< 运行模式："serial" 或 "parallel"
    std::string parallel_mode_;           ///< 并行子模式："independent" 或 "sync"
    size_t batch_size_;                   ///< 批处理大小

    /**
     * @brief 并行同步模式专用变量
     *
     * @section 同步模式执行流程
     *
     * ┌─────────────────────────────────────────────────────────────────────────────┐
     * │                       同步模式线程协作流程                                   │
     * └─────────────────────────────────────────────────────────────────────────────┘
     *
     *  帧分发线程                    Model1 线程                    Model2 线程
     *  ──────────────              ────────────                  ────────────
     *
     *  获取原始帧 raw
     *       │
     *       │ 复制到 shared_frame_
     *       │ frame_sequence_++
     *       │ completed_count_ = 0
     *       │
     *  for each model:
     *       │ frame_ready_event_.set(1)
     *       │◄──────────────────────┤
     *       │                       │
     *       │                       │ frame_ready_event_.wait()
     *       │                       │ (阻塞等待)
     *       │                       │
     *       │                       │ 复制 shared_frame_ 到 local_frame_
     *       │                       │ completed_count_++
     *       │                       │
     *       │                       │ if completed_count_ >= N:
     *       │                       │     all_done_event_.set(1)
     *       │                       │
     *       │                       │ 开始模型推理...
     *       │                       │
     *       │ all_done_event_.wait()
     *       │ (阻塞等待所有模型)
     *       │
     *  释放 raw 帧
     *  获取下一帧...
     *
     * @section 同步原语详细说明
     *
     * 1. frame_sequence_ (int)
     *    - 用途：标识当前处理的帧序号，用于跟踪帧分发进度
     *    - 更新时机：帧分发线程在分发新帧前递增
     *
     * 2. completed_count_ (int)
     *    - 用途：记录已完成当前帧复制的模型数量
     *    - 初始值：0
     *    - 递增时机：每个模型线程完成 shared_frame_ 到 local_frame_ 复制后
     *
     * 3. sync_mutex_ (Mutex)
     *    - 用途：保护 shared_frame_ 和 completed_count_ 的线程安全访问
     *    - 获取时机：帧分发线程写入 shared_frame_ 时；模型线程读取 shared_frame_ 时
     *    - 释放时机：操作完成后立即释放
     *
     * 4. frame_ready_event_ (Event)
     *    - 用途：帧分发线程通知模型线程有新帧可处理
     *    - 设置时机：帧分发线程将帧复制到共享缓冲区后
     *    - 等待时机：模型线程在每个推理循环开始时等待
     *    - 特点：自动重置事件，每次设置后只唤醒一个等待者（如需唤醒多个需多次set）
     *
     * 5. all_done_event_ (Event)
     *    - 用途：模型线程通知帧分发线程所有模型已完成当前帧处理
     *    - 设置时机：最后一个模型完成帧复制后
     *    - 等待时机：帧分发线程在分发帧后等待
     *    - 超时处理：若等待超时，重置 completed_count_ 继续运行
     *
     * 6. shared_frame_ (cv2::Mat)
     *    - 用途：帧分发线程与模型线程之间的共享帧缓冲区
     *    - 生命周期：由帧分发线程统一管理释放
     *    - 访问方式：在 sync_mutex_ 保护下进行读写
     *
     * 7. local_frame_ (cv2::Mat)
     *    - 用途：模型线程的本地帧拷贝，与 shared_frame_ 解耦
     *    - 复制时机：模型线程收到 frame_ready_event_ 后
     *    - 使用方式：用于后续的图像预处理和模型推理
     *
     * @section 共享帧管理说明
     *
     * 共享帧的生命周期管理：
     *  1. 帧分发线程从摄像头获取 raw 帧
     *  2. 复制 raw->img.data 到 shared_frame_（在 sync_mutex_ 保护下）
     *  3. 设置 frame_ready_event_ 唤醒所有模型线程
     *  4. 等待 all_done_event_ 确认所有模型完成复制
     *  5. 释放 raw 帧（不释放 shared_frame_，因为模型线程已复制）
     *
     * 模型线程的本地帧管理：
     *  1. 等待 frame_ready_event_
     *  2. 在 sync_mutex_ 保护下复制 shared_frame_ 到 local_frame_
     *  3. 增加 completed_count_
     *  4. 如果是最后一个模型，设置 all_done_event_
     *  5. 使用 local_frame_ 进行后续处理（与共享帧解耦）
     *
     * @note 这种设计确保：
     *       - 帧分发线程和模型线程可以并发运行
     *       - 所有模型处理的是同一帧的拷贝，保证时间戳一致性
     *       - 共享帧的释放由帧分发线程统一管理，避免悬空引用
     */
    int frame_sequence_;                  ///< 当前帧序号（同步模式）
    int completed_count_;                 ///< 已完成当前帧的模型数量（同步模式）
    Mutex sync_mutex_;                    ///< 同步机制互斥锁（同步模式）
    Event frame_ready_event_;             ///< 帧就绪事件（同步模式）
    Event all_done_event_;                ///< 所有模型完成事件（同步模式）
    cv2::Mat shared_frame_;               ///< 共享帧缓冲区（同步模式）
    cv2::Mat local_frame_;                ///< 模型线程本地帧拷贝（同步模式）

    std::vector<std::vector<float>> tensor_pool_;  ///< Tensor数据对象池（避免频繁new/delete）
    Mutex tensor_pool_mutex_;             ///< 对象池互斥锁

    static constexpr size_t MAX_BASE64_SIZE = 100 * 1024;  ///< Base64编码最大JPEG尺寸限制
    static constexpr size_t MAX_TENSOR_POOL_SIZE = 10;     ///< 对象池最大缓冲数量

    std::vector<float>* getTensorBuffer(size_t size);      ///< 从对象池获取Tensor缓冲区
    void returnTensorBuffer(std::vector<float>* buffer);   ///< 将缓冲区归还到对象池

};
}  // namespace ma::node