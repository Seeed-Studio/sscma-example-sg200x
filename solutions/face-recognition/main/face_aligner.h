#pragma once
#include <cstdint>
#include <cmath>
#include <vector>

// ArcFace standard 5-point reference landmarks for 112x112
static constexpr float ARCFACE_REF[5][2] = {
    {38.2946f, 51.6963f},  // left eye
    {73.5318f, 51.5014f},  // right eye
    {56.0252f, 71.7366f},  // nose tip
    {41.5493f, 92.3655f},  // left mouth corner
    {70.7299f, 92.2041f},  // right mouth corner
};

struct FaceAligner {
    // Compute similarity transform from detected landmarks to ArcFace reference
    // src_lm: detected 5 landmarks [5][2] in original image
    // Returns 2x3 affine matrix (forward: src -> 112x112)
    static void computeTransform(const float src_lm[5][2], float M[6]);

    // Apply inverse warp: for each pixel in 112x112 dst, sample from src
    // M: forward transform (src -> 112x112)
    // src_bgr: source image BGR planar (3, H, W) uint8
    // dst_rgb: output 112x112x3 RGB interleaved uint8
    static void align(const uint8_t* src_bgr, int src_h, int src_w,
                      const float M[6], uint8_t* dst_rgb);

    // Invert 2x3 affine transform
    static void invertTransform(const float M[6], float Mi[6]);
};
