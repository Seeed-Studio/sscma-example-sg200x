#include "cviruntime.h"
#include <math.h>
#include <opencv2/opencv.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "person.h"

double __get_us(struct timeval t) {
    return (t.tv_sec * 1000000 + t.tv_usec);
}

typedef struct {
    float x, y, w, h;
} box;

typedef struct {
    box bbox;
    int cls;
    float score;
    int batch_idx;
} detection;

static const char* coco_names[] = {
    "person",        "bicycle",       "car",           "motorbike",
    "aeroplane",     "bus",           "train",         "truck",
    "boat",          "traffic light", "fire hydrant",  "stop sign",
    "parking meter", "bench",         "bird",          "cat",
    "dog",           "horse",         "sheep",         "cow",
    "elephant",      "bear",          "zebra",         "giraffe",
    "backpack",      "umbrella",      "handbag",       "tie",
    "suitcase",      "frisbee",       "skis",          "snowboard",
    "sports ball",   "kite",          "baseball bat",  "baseball glove",
    "skateboard",    "surfboard",     "tennis racket", "bottle",
    "wine glass",    "cup",           "fork",          "knife",
    "spoon",         "bowl",          "banana",        "apple",
    "sandwich",      "orange",        "broccoli",      "carrot",
    "hot dog",       "pizza",         "donut",         "cake",
    "chair",         "sofa",          "pottedplant",   "bed",
    "diningtable",   "toilet",        "tvmonitor",     "laptop",
    "mouse",         "remote",        "keyboard",      "cell phone",
    "microwave",     "oven",          "toaster",       "sink",
    "refrigerator",  "book",          "clock",         "vase",
    "scissors",      "teddy bear",    "hair drier",    "toothbrush"};

static void usage(char** argv) {
    printf("Usage:\n");
    printf("   %s cvimodel image.jpg image_detected.jpg\n", argv[0]);
    printf("ex: %s yolov8n_int8_fuse_preprocess.cvimodel cat.jpg out.jpg \n", argv[0]);
}

static void dump_tensors(CVI_TENSOR* tensors, int32_t num) {
    for (int32_t i = 0; i < num; i++) {
        printf("  [%d] %s, shape (%d,%d,%d,%d), count %zu, fmt %d\n",
               i,
               tensors[i].name,
               tensors[i].shape.dim[0],
               tensors[i].shape.dim[1],
               tensors[i].shape.dim[2],
               tensors[i].shape.dim[3],
               tensors[i].count,
               tensors[i].fmt);
    }
}

template <typename T>
int argmax(const T* data, size_t len, size_t stride = 1) {
    int maxIndex = 0;
    for (size_t i = stride; i < len; i += stride) {
        if (data[maxIndex] < data[i]) {
            maxIndex = i;
        }
    }
    return maxIndex;
}

static inline float sigmoid(float x) {
    return static_cast<float>(1.f / (1.f + exp(-x)));
}


float calIou(box a, box b) {
    float area1 = a.w * a.h;
    float area2 = b.w * b.h;
    float wi =
        std::min((a.x + a.w / 2), (b.x + b.w / 2)) - std::max((a.x - a.w / 2), (b.x - b.w / 2));
    float hi =
        std::min((a.y + a.h / 2), (b.y + b.h / 2)) - std::max((a.y - a.h / 2), (b.y - b.h / 2));
    float area_i = std::max(wi, 0.0f) * std::max(hi, 0.0f);
    return area_i / (area1 + area2 - area_i);
}

static void NMS(std::vector<detection>& dets, int* total, float thresh) {
    if (*total) {
        std::sort(
            dets.begin(), dets.end(), [](detection& a, detection& b) { return b.score < a.score; });
        int new_count = *total;
        for (int i = 0; i < *total; ++i) {
            detection& a = dets[i];
            if (a.score == 0)
                continue;
            for (int j = i + 1; j < *total; ++j) {
                detection& b = dets[j];
                if (dets[i].batch_idx == dets[j].batch_idx && b.score != 0 &&
                    dets[i].cls == dets[j].cls && calIou(a.bbox, b.bbox) > thresh) {
                    b.score = 0;
                    new_count--;
                }
            }
        }
        std::vector<detection>::iterator it = dets.begin();
        while (it != dets.end()) {
            if (it->score == 0) {
                dets.erase(it);
            } else {
                it++;
            }
        }
        *total = new_count;
    }
}

void correctYoloBoxes(std::vector<detection>& dets,
                      int det_num,
                      int image_h,
                      int image_w,
                      int input_height,
                      int input_width) {
    int restored_w;
    int restored_h;
    float resize_ratio;
    if (((float)input_width / image_w) < ((float)input_height / image_h)) {
        restored_w = input_width;
        restored_h = (image_h * input_width) / image_w;
    } else {
        restored_h = input_height;
        restored_w = (image_w * input_height) / image_h;
    }
    resize_ratio = ((float)image_w / restored_w);

    for (int i = 0; i < det_num; ++i) {
        box bbox = dets[i].bbox;
        int b    = dets[i].batch_idx;
        bbox.x   = (bbox.x - (input_width - restored_w) / 2.) * resize_ratio;
        bbox.y   = (bbox.y - (input_height - restored_h) / 2.) * resize_ratio;
        bbox.w *= resize_ratio;
        bbox.h *= resize_ratio;
        dets[i].bbox = bbox;
    }
}

void compute_dfl(float* tensor, int dfl_len, float* box) {
    for (int b = 0; b < 4; b++) {
        float exp_t[dfl_len];
        float exp_sum = 0;
        float acc_sum = 0;
        for (int i = 0; i < dfl_len; i++) {
            exp_t[i] = exp(tensor[i + b * dfl_len]);
            exp_sum += exp_t[i];
        }

        for (int i = 0; i < dfl_len; i++) {
            acc_sum += exp_t[i] / exp_sum * i;
        }
        box[b] = acc_sum;
    }
}

static int getDetections(CVI_TENSOR* output,
                         CVI_SHAPE* output_shape,
                         int32_t output_num,
                         int32_t height,
                         int classes_num,
                         float conf_thresh,
                         std::vector<detection>& dets) {
    int validCount = 0;
    int stride     = 0;
    int grid_h     = 0;
    int grid_w     = 0;

    int dfl_len = output_shape[0].dim[1] / 4;  // 16

    int batch = output_shape[0].dim[0];
    for (int b = 0; b < batch; b++) {
        // batch=1  1*64*80*80 1*64*40*40 1*64*20*20 1*80*80*80 1*80*40*40 1*80*20*20
        for (int a = 0; a < 3; a++) {
            int box_idx         = a * 2;
            int score_idx       = box_idx + 1;
            float* box_tensor   = (float*)CVI_NN_TensorPtr(&output[box_idx]);
            float* score_tensor = (float*)CVI_NN_TensorPtr(&output[score_idx]);

            grid_h = output_shape[box_idx].dim[2];
            grid_w = output_shape[box_idx].dim[3];

            int grid_len = grid_h * grid_w;

            stride = height / grid_h;  // 8 16 32

            for (int i = 0; i < grid_h; i++) {
                for (int j = 0; j < grid_w; j++) {
                    int offset       = i * grid_w + j;
                    int max_class_id = -1;

                    float max_score = 0;
                    float score     = 0;
                    for (int c = 0; c < classes_num; c++) {
                        score = sigmoid(score_tensor[offset]);
                        if ((score > conf_thresh) && (score > max_score)) {
                            max_score    = score;
                            max_class_id = c;
                        }
                        offset += grid_len;
                    }

                    // compute box
                    if (max_score > conf_thresh) {
                        offset = i * grid_w + j;
                        float box[4];
                        float before_dfl[dfl_len * 4];
                        for (int k = 0; k < dfl_len * 4; k++) {
                            before_dfl[k] = box_tensor[offset];
                            offset += grid_len;
                        }
                        compute_dfl(before_dfl, dfl_len, box);

                        float x1, y1, x2, y2, w, h;
                        x1 = (-box[0] + j + 0.5) * stride;
                        y1 = (-box[1] + i + 0.5) * stride;
                        x2 = (box[2] + j + 0.5) * stride;
                        y2 = (box[3] + i + 0.5) * stride;
                        w  = x2 - x1;
                        h  = y2 - y1;

                        detection det;
                        det.score     = max_score;
                        det.cls       = max_class_id;
                        det.batch_idx = 0;
                        validCount++;

                        det.bbox.h = h;
                        det.bbox.w = w;
                        det.bbox.x = x1 + det.bbox.w / 2.0;
                        det.bbox.y = y1 + det.bbox.h / 2.0;
                        dets.emplace_back(det);
                    }
                }
            }
        }
    }
    return validCount;
}

int main(int argc, char** argv) {
    int ret = 0;
    CVI_MODEL_HANDLE model;
    struct timeval start_time, stop_time;

    if (argc != 4) {
        usage(argv);
        exit(-1);
    }
    CVI_TENSOR* input;
    CVI_TENSOR* output;
    CVI_TENSOR* input_tensors;
    CVI_TENSOR* output_tensors;
    int32_t input_num;
    int32_t output_num;
    CVI_SHAPE input_shape;
    CVI_SHAPE* output_shape;
    int32_t height;
    int32_t width;

    // int bbox_len = 84; // classes num + 4
    int classes_num   = 80;
    float conf_thresh = 0.5;
    float iou_thresh  = 0.5;
    ret               = CVI_NN_RegisterModel(argv[1], &model);
    if (ret != CVI_RC_SUCCESS) {
        printf("CVI_NN_RegisterModel failed, err %d\n", ret);
        exit(1);
    }
    printf("CVI_NN_RegisterModel succeeded\n");

    // get input output tensors
    CVI_NN_GetInputOutputTensors(model, &input_tensors, &input_num, &output_tensors, &output_num);
    printf("Input Tensor Number  : %d\n", input_num);
    dump_tensors(input_tensors, input_num);
    printf("Output Tensor Number : %d\n", output_num);
    dump_tensors(output_tensors, output_num);

    input = CVI_NN_GetTensorByName(CVI_NN_DEFAULT_TENSOR, input_tensors, input_num);
    assert(input);
    output       = output_tensors;
    output_shape = reinterpret_cast<CVI_SHAPE*>(calloc(output_num, sizeof(CVI_SHAPE)));
    for (int i = 0; i < output_num; i++) {
        output_shape[i] = CVI_NN_TensorShape(&output[i]);
    }

    // nchw
    input_shape = CVI_NN_TensorShape(input);
    height      = input_shape.dim[1];
    width       = input_shape.dim[2];
    assert(height % 32 == 0 && width % 32 == 0);
    // imread
    cv::Mat image;
    image = cv::imread(argv[2]);
    if (!image.data) {
        printf("Could not open or find the image\n");
        return -1;
    }
    cv::Mat cloned = image.clone();

    // resize & letterbox
    int ih              = image.rows;
    int iw              = image.cols;
    int oh              = height;
    int ow              = width;
    double resize_scale = std::min((double)oh / ih, (double)ow / iw);
    int nh              = (int)(ih * resize_scale);
    int nw              = (int)(iw * resize_scale);
    cv::resize(image, image, cv::Size(nw, nh));
    int top    = (oh - nh) / 2;
    int bottom = (oh - nh) - top;
    int left   = (ow - nw) / 2;
    int right  = (ow - nw) - left;
    cv::copyMakeBorder(
        image, image, top, bottom, left, right, cv::BORDER_CONSTANT, cv::Scalar::all(0));
    cv::cvtColor(image, image, cv::COLOR_BGR2RGB);

    // Packed2Planar
    // cv::Mat channels[3];
    // for (int i = 0; i < 3; i++) {
    //     channels[i] = cv::Mat(image.rows, image.cols, CV_8SC1);
    // }
    // cv::split(image, channels);

    // fill data
    int8_t* ptr      = (int8_t*)CVI_NN_TensorPtr(input);
    // int channel_size = height * width;
    // for (int i = 0; i < 3; ++i) {
    //     memcpy(ptr + i * channel_size, channels[i].data, channel_size);
    // }
    memcpy(ptr,  gImage_person, sizeof(gImage_person));

    // run inference
    gettimeofday(&start_time, NULL);
    CVI_NN_Forward(model, input_tensors, input_num, output_tensors, output_num);
    gettimeofday(&stop_time, NULL);
    printf("CVI_NN_Forward using %f ms\n", (__get_us(stop_time) - __get_us(start_time)) / 1000);
    printf("CVI_NN_Forward Succeed...\n");
    // do post proprocess
    int det_num = 0;
    std::vector<detection> dets;
    det_num =
        getDetections(output, output_shape, output_num, height, classes_num, conf_thresh, dets);

    // correct box with origin image size
    NMS(dets, &det_num, iou_thresh);
    correctYoloBoxes(dets, det_num, cloned.rows, cloned.cols, height, width);

    // draw bbox on image
    printf("-------------------\n");
    printf("%d objects are detected\n", det_num);
    for (int i = 0; i < det_num; i++) {
        box b = dets[i].bbox;
        // xywh2xyxy
        int x1 = (b.x - b.w / 2);
        int y1 = (b.y - b.h / 2);
        int x2 = (b.x + b.w / 2);
        int y2 = (b.y + b.h / 2);
        cv::rectangle(
            cloned, cv::Point(x1, y1), cv::Point(x2, y2), cv::Scalar(255, 255, 0), 3, 8, 0);
        char content[100];
        printf("%s @ (%d %d %d %d) %.3f\n", coco_names[dets[i].cls], x1, y1, x2, y2, dets[i].score);
        sprintf(content, "%s %0.3f", coco_names[dets[i].cls], dets[i].score);
        cv::putText(cloned,
                    content,
                    cv::Point(x1, y1),
                    cv::FONT_HERSHEY_DUPLEX,
                    1.0,
                    cv::Scalar(0, 0, 255),
                    2);
    }
    printf("-------------------\n");

    // save or show picture
    cv::imwrite(argv[3], cloned);

    CVI_NN_CleanupModel(model);
    printf("CVI_NN_CleanupModel succeeded\n");
    free(output_shape);
    return 0;
}