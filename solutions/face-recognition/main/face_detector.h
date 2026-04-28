#pragma once
#include <cstdint>
#include <vector>
#include <forward_list>

struct FaceLandmark {
    float x, y;
};

struct FaceBox {
    float x1, y1, x2, y2;
    float confidence;
    FaceLandmark landmarks[5];
};

struct SCRFDParams {
    int input_w = 640;
    int input_h = 640;
    int num_anchors = 2;
    int strides[3] = {8, 16, 32};
    float conf_threshold = 0.5f;
    float nms_threshold = 0.4f;
};

class FaceDetector {
public:
    FaceDetector(const SCRFDParams& params = {});
    ~FaceDetector();

    // Decode SCRFD outputs (9 tensors) into detected faces
    // outputs: pointer to 9 output arrays (score0, score1, score2, bbox0, bbox1, bbox2, kps0, kps1, kps2)
    // output_shapes: [9][2] array of {rows, cols} for each output
    // scale: ratio to map from model coords to original image coords
    // pad_w, pad_h: letterbox padding offsets
    std::vector<FaceBox> detect(const float* outputs[9], const int output_shapes[9][2],
                                float scale, float pad_w, float pad_h);

private:
    SCRFDParams params_;

    static float iou(const FaceBox& a, const FaceBox& b);
    std::vector<FaceBox> nms(std::vector<FaceBox>& faces);
};
