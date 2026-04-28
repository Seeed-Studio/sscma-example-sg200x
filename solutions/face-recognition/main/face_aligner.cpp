#include "face_aligner.h"
#include <cstring>
#include <algorithm>

void FaceAligner::computeTransform(const float src_lm[5][2], float M[6]) {
    // Use eye centers for rotation/scale, centroid for translation
    float src_eye_lx = src_lm[0][0], src_eye_ly = src_lm[0][1];
    float src_eye_rx = src_lm[1][0], src_eye_ry = src_lm[1][1];

    float ref_eye_lx = ARCFACE_REF[0][0], ref_eye_ly = ARCFACE_REF[0][1];
    float ref_eye_rx = ARCFACE_REF[1][0], ref_eye_ry = ARCFACE_REF[1][1];

    // Angle from eye vector
    float src_angle = atan2f(src_eye_ry - src_eye_ly, src_eye_rx - src_eye_lx);
    float ref_angle = atan2f(ref_eye_ry - ref_eye_ly, ref_eye_rx - ref_eye_lx);
    float rotation = ref_angle - src_angle;

    // Scale from inter-ocular distance
    float src_dist = hypotf(src_eye_rx - src_eye_lx, src_eye_ry - src_eye_ly);
    float ref_dist = hypotf(ref_eye_rx - ref_eye_lx, ref_eye_ry - ref_eye_ly);
    float scale = ref_dist / src_dist;

    float cos_r = cosf(rotation) * scale;
    float sin_r = sinf(rotation) * scale;

    // Rotation center: midpoint of detected eyes
    float src_cx = (src_eye_lx + src_eye_rx) * 0.5f;
    float src_cy = (src_eye_ly + src_eye_ry) * 0.5f;
    float ref_cx = (ref_eye_lx + ref_eye_rx) * 0.5f;
    float ref_cy = (ref_eye_ly + ref_eye_ry) * 0.5f;

    // Forward: src -> 112x112
    // x' = cos_r * (x - src_cx) - sin_r * (y - src_cy) + ref_cx
    // y' = sin_r * (x - src_cx) + cos_r * (y - src_cy) + ref_cy
    M[0] = cos_r;
    M[1] = -sin_r;
    M[2] = ref_cx - cos_r * src_cx + sin_r * src_cy;
    M[3] = sin_r;
    M[4] = cos_r;
    M[5] = ref_cy - sin_r * src_cx - cos_r * src_cy;
}

void FaceAligner::invertTransform(const float M[6], float Mi[6]) {
    // Invert 2x2 part
    float a = M[0], b = M[1], c = M[3], d = M[4];
    float tx = M[2], ty = M[5];
    float det = a * d - b * c;
    float inv_det = 1.0f / det;

    Mi[0] = d * inv_det;
    Mi[1] = -b * inv_det;
    Mi[2] = (b * ty - d * tx) * inv_det;
    Mi[3] = -c * inv_det;
    Mi[4] = a * inv_det;
    Mi[5] = (c * tx - a * ty) * inv_det;
}

void FaceAligner::align(const uint8_t* src_bgr, int src_h, int src_w,
                        const float M[6], uint8_t* dst_rgb) {
    float Mi[6];
    invertTransform(M, Mi);

    // src_bgr is BGR planar: channel B at offset 0, G at H*W, R at 2*H*W
    int plane = src_h * src_w;

    for (int y = 0; y < 112; y++) {
        for (int x = 0; x < 112; x++) {
            // Inverse map: dst(x,y) -> src(sx,sy)
            float sx = Mi[0] * x + Mi[1] * y + Mi[2];
            float sy = Mi[3] * x + Mi[4] * y + Mi[5];

            int ix = (int)floorf(sx);
            int iy = (int)floorf(sy);
            float fx = sx - ix;
            float fy = sy - iy;

            int out_idx = (y * 112 + x) * 3;

            if (ix < 0 || ix + 1 >= src_w || iy < 0 || iy + 1 >= src_h) {
                dst_rgb[out_idx + 0] = 0;
                dst_rgb[out_idx + 1] = 0;
                dst_rgb[out_idx + 2] = 0;
                continue;
            }

            // Bilinear interpolation from BGR planar -> RGB interleaved
            // src_bgr layout: B plane, G plane, R plane
            int idx00 = iy * src_w + ix;
            int idx10 = idx00 + 1;
            int idx01 = idx00 + src_w;
            int idx11 = idx01 + 1;

            float w00 = (1 - fx) * (1 - fy);
            float w10 = fx * (1 - fy);
            float w01 = (1 - fx) * fy;
            float w11 = fx * fy;

            // B channel
            float b_val = src_bgr[idx00] * w00 + src_bgr[idx10] * w10 +
                          src_bgr[idx01] * w01 + src_bgr[idx11] * w11;
            // G channel
            float g_val = src_bgr[plane + idx00] * w00 + src_bgr[plane + idx10] * w10 +
                          src_bgr[plane + idx01] * w01 + src_bgr[plane + idx11] * w11;
            // R channel
            float r_val = src_bgr[2 * plane + idx00] * w00 + src_bgr[2 * plane + idx10] * w10 +
                          src_bgr[2 * plane + idx01] * w01 + src_bgr[2 * plane + idx11] * w11;

            // Output RGB order
            dst_rgb[out_idx + 0] = (uint8_t)std::min(255.0f, std::max(0.0f, r_val));
            dst_rgb[out_idx + 1] = (uint8_t)std::min(255.0f, std::max(0.0f, g_val));
            dst_rgb[out_idx + 2] = (uint8_t)std::min(255.0f, std::max(0.0f, b_val));
        }
    }
}
