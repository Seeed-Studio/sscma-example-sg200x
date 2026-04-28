#include "face_detector.h"
#include <algorithm>
#include <cmath>

FaceDetector::FaceDetector(const SCRFDParams& params) : params_(params) {}
FaceDetector::~FaceDetector() = default;

float FaceDetector::iou(const FaceBox& a, const FaceBox& b) {
    float x1 = std::max(a.x1, b.x1);
    float y1 = std::max(a.y1, b.y1);
    float x2 = std::min(a.x2, b.x2);
    float y2 = std::min(a.y2, b.y2);
    float inter = std::max(0.0f, x2 - x1) * std::max(0.0f, y2 - y1);
    float area_a = (a.x2 - a.x1) * (a.y2 - a.y1);
    float area_b = (b.x2 - b.x1) * (b.y2 - b.y1);
    return inter / (area_a + area_b - inter + 1e-6f);
}

std::vector<FaceBox> FaceDetector::nms(std::vector<FaceBox>& faces) {
    std::sort(faces.begin(), faces.end(),
              [](const FaceBox& a, const FaceBox& b) { return a.confidence > b.confidence; });

    std::vector<bool> suppressed(faces.size(), false);
    std::vector<FaceBox> result;

    for (size_t i = 0; i < faces.size(); i++) {
        if (suppressed[i]) continue;
        result.push_back(faces[i]);
        for (size_t j = i + 1; j < faces.size(); j++) {
            if (suppressed[j]) continue;
            if (iou(faces[i], faces[j]) > params_.nms_threshold) {
                suppressed[j] = true;
            }
        }
    }
    return result;
}

std::vector<FaceBox> FaceDetector::detect(const float* outputs[9], const int output_shapes[9][2],
                                           float scale, float pad_w, float pad_h) {
    std::vector<FaceBox> all_faces;

    for (int s = 0; s < 3; s++) {
        int stride = params_.strides[s];
        int rows = output_shapes[s][0];   // e.g., 12800, 3200, 800
        int num_score = output_shapes[s][1];  // should be 1
        int bbox_cols = output_shapes[s + 3][1];  // should be 4
        int kps_cols = output_shapes[s + 6][1];   // should be 10

        int grid_total = rows / params_.num_anchors;
        int grid_h = grid_total;  // We'll compute per-anchor

        // Estimate grid size from total anchors and stride
        int feat_h = params_.input_h / stride;
        int feat_w = params_.input_w / stride;

        const float* scores = outputs[s];
        const float* bbox = outputs[s + 3];
        const float* kps = outputs[s + 6];

        for (int idx = 0; idx < rows; idx++) {
            float score = scores[idx * num_score];
            if (score < params_.conf_threshold) continue;

            int anchor = idx % params_.num_anchors;
            int spatial = idx / params_.num_anchors;
            int fy = spatial / feat_w;
            int fx = spatial % feat_w;

            // Anchor center in model coordinates
            float cx = (fx * (float)stride + anchor * 0.5f * stride);
            float cy = (fy * (float)stride + anchor * 0.5f * stride);

            // Distance-based bbox decode
            float d_left   = bbox[idx * bbox_cols + 0] * stride;
            float d_top    = bbox[idx * bbox_cols + 1] * stride;
            float d_right  = bbox[idx * bbox_cols + 2] * stride;
            float d_bottom = bbox[idx * bbox_cols + 3] * stride;

            FaceBox face;
            face.x1 = (cx - d_left - pad_w) * scale;
            face.y1 = (cy - d_top - pad_h) * scale;
            face.x2 = (cx + d_right - pad_w) * scale;
            face.y2 = (cy + d_bottom - pad_h) * scale;
            face.confidence = score;

            // Keypoints decode (5 points x 2 coords = 10 values)
            for (int k = 0; k < 5; k++) {
                face.landmarks[k].x = (cx + kps[idx * kps_cols + k * 2] * stride - pad_w) * scale;
                face.landmarks[k].y = (cy + kps[idx * kps_cols + k * 2 + 1] * stride - pad_h) * scale;
            }

            all_faces.push_back(face);
        }
    }

    return nms(all_faces);
}
