#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <cmath>

#include <opencv2/opencv.hpp>

#include <sscma.h>

#include "face_detector.h"
#include "face_aligner.h"
#include "face_database.h"

// ── Globals ──
static ma::engine::EngineCVI* g_scrfd_engine = nullptr;
static ma::engine::EngineCVI* g_emb_engine = nullptr;
static FaceDetector g_detector;
static FaceDatabase g_facedb;

// ── Embedding extraction helpers ──
static void l2_normalize(float* v, int dim) {
    float norm = 0;
    for (int i = 0; i < dim; i++) norm += v[i] * v[i];
    norm = sqrtf(norm + 1e-10f);
    if (norm > 1e-10f) {
        for (int i = 0; i < dim; i++) v[i] /= norm;
    }
}

static std::vector<float> extractEmbedding(const uint8_t* aligned_rgb_112) {
    // Feed aligned 112x112 RGB to MobileFaceNet
    auto& input = g_emb_engine->getInput(0);
    memcpy(input.data.u8, aligned_rgb_112, 112 * 112 * 3);
    g_emb_engine->run(0);

    // Find the 128D float32 embedding output
    int num_outputs = g_emb_engine->getOutputSize();
    for (int i = 0; i < num_outputs; i++) {
        auto& out = g_emb_engine->getOutput(i);
        if (out.type != MA_TENSOR_TYPE_F32) continue;
        int total = 1;
        for (int d = 0; d < out.shape.size; d++) total *= out.shape.dims[d];
        if (total == 128) {
            std::vector<float> emb(128);
            memcpy(emb.data(), out.data.f32, 128 * sizeof(float));
            l2_normalize(emb.data(), 128);
            return emb;
        }
    }
    fprintf(stderr, "ERROR: No 128D float32 embedding output found\n");
    return std::vector<float>(128, 0.0f);
}

// ── SCRFD inference ──
static std::vector<FaceBox> detectFaces(cv::Mat& image) {
    // Preprocess: resize to 640x640
    float scale_w = 640.0f / image.cols;
    float scale_h = 640.0f / image.rows;
    float scale = std::min(scale_w, scale_h);
    int new_w = (int)(image.cols * scale);
    int new_h = (int)(image.rows * scale);

    cv::Mat resized;
    cv::resize(image, resized, cv::Size(new_w, new_h));

    // Letterbox padding
    int pad_w = (640 - new_w) / 2;
    int pad_h = (640 - new_h) / 2;
    cv::Mat padded(640, 640, CV_8UC3, cv::Scalar(0, 0, 0));
    resized.copyTo(padded(cv::Rect(pad_w, pad_h, new_w, new_h)));

    // Convert BGR to RGB
    cv::Mat rgb;
    cv::cvtColor(padded, rgb, cv::COLOR_BGR2RGB);

    // Feed to SCRFD engine
    auto& input = g_scrfd_engine->getInput(0);
    memcpy(input.data.u8, rgb.data, 640 * 640 * 3);
    g_scrfd_engine->run(0);

    // Find the 9 _f32 output tensors by shape
    // Score: (N, 1, 1, 1) float32  — already sigmoided
    // BBox:  (N, 4, 1, 1) float32
    // Kps:   (N, 10, 1, 1) float32
    // Where N = 12800 (stride 8), 3200 (stride 16), 800 (stride 32)
    struct OutRef { const float* data; int d0, d1; };
    std::vector<OutRef> scores, bboxes, kps_list;

    int num_outputs = g_scrfd_engine->getOutputSize();
    for (int i = 0; i < num_outputs; i++) {
        auto& out = g_scrfd_engine->getOutput(i);
        if (out.type != MA_TENSOR_TYPE_F32) continue;
        if (out.shape.size < 2) continue;
        int d0 = out.shape.dims[0];
        int d1 = out.shape.dims[1];
        if (d0 != 12800 && d0 != 3200 && d0 != 800) continue;
        if (d1 == 1)       scores.push_back({out.data.f32, d0, d1});
        else if (d1 == 4)  bboxes.push_back({out.data.f32, d0, d1});
        else if (d1 == 10) kps_list.push_back({out.data.f32, d0, d1});
    }

    // Sort by d0 descending to match strides 8, 16, 32
    auto cmp = [](const OutRef& a, const OutRef& b) { return a.d0 > b.d0; };
    std::sort(scores.begin(), scores.end(), cmp);
    std::sort(bboxes.begin(), bboxes.end(), cmp);
    std::sort(kps_list.begin(), kps_list.end(), cmp);

    if (scores.size() != 3 || bboxes.size() != 3 || kps_list.size() != 3) {
        fprintf(stderr, "ERROR: Expected 3 score + 3 bbox + 3 kps outputs, got %zu/%zu/%zu\n",
                scores.size(), bboxes.size(), kps_list.size());
        return {};
    }

    const float* out_data[9] = {
        scores[0].data, scores[1].data, scores[2].data,
        bboxes[0].data, bboxes[1].data, bboxes[2].data,
        kps_list[0].data, kps_list[1].data, kps_list[2].data,
    };
    int out_shapes[9][2] = {};
    for (int i = 0; i < 3; i++) {
        out_shapes[i][0]   = scores[i].d0;   out_shapes[i][1]   = scores[i].d1;
        out_shapes[i+3][0] = bboxes[i].d0;   out_shapes[i+3][1] = bboxes[i].d1;
        out_shapes[i+6][0] = kps_list[i].d0; out_shapes[i+6][1] = kps_list[i].d1;
    }

    float inv_scale = 1.0f / scale;
    return g_detector.detect(out_data, out_shapes, inv_scale, (float)pad_w, (float)pad_h);
}

// ── Draw results ──
static void drawFaces(cv::Mat& img, const std::vector<FaceBox>& faces, const std::string& name = "", float score = 0) {
    for (const auto& f : faces) {
        cv::rectangle(img, cv::Point(f.x1, f.y1), cv::Point(f.x2, f.y2), cv::Scalar(0, 255, 0), 2);

        // Draw landmarks
        for (int i = 0; i < 5; i++) {
            cv::circle(img, cv::Point(f.landmarks[i].x, f.landmarks[i].y), 2, cv::Scalar(0, 0, 255), -1);
        }

        // Label
        char label[128];
        if (!name.empty()) {
            snprintf(label, sizeof(label), "%s (%.2f)", name.c_str(), score);
        } else {
            snprintf(label, sizeof(label), "%.2f", f.confidence);
        }
        cv::putText(img, label, cv::Point(f.x1, f.y1 - 8),
                    cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 0), 2);
    }
}

// ── Usage ──
static void printUsage(const char* prog) {
    printf("Face Recognition for CV181x TPU\n\n");
    printf("Usage:\n");
    printf("  %s register <scrfd.cvimodel> <facenet.cvimodel> <photo.jpg> <name> <facedb.txt>\n", prog);
    printf("  %s identify <scrfd.cvimodel> <facenet.cvimodel> <photo.jpg> <facedb.txt> [-o result.jpg]\n", prog);
    printf("  %s detect   <scrfd.cvimodel> <photo.jpg> [-o result.jpg]\n", prog);
    printf("  %s list     <facedb.txt>\n", prog);
}

// ── Main ──
int main(int argc, char** argv) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string cmd = argv[1];

    // ── List command (no model needed) ──
    if (cmd == "list") {
        if (argc < 3) { printUsage(argv[0]); return 1; }
        FaceDatabase db;
        if (!db.load(argv[2])) {
            printf("Failed to load face database: %s\n", argv[2]);
            return 1;
        }
        db.list();
        return 0;
    }

    // ── Detect command (SCRFD only) ──
    if (cmd == "detect") {
        if (argc < 4) { printUsage(argv[0]); return 1; }

        g_scrfd_engine = new ma::engine::EngineCVI();
        g_scrfd_engine->init();
        g_scrfd_engine->load(argv[2]);

        cv::Mat img = cv::imread(argv[3]);
        if (img.empty()) { printf("Failed to read: %s\n", argv[3]); return 1; }

        auto faces = detectFaces(img);
        printf("Detected %zu faces\n", faces.size());
        for (const auto& f : faces) {
            printf("  bbox: [%.0f, %.0f, %.0f, %.0f] conf=%.3f\n", f.x1, f.y1, f.x2, f.y2, f.confidence);
        }

        // Save result
        drawFaces(img, faces);

        std::string out_path = "result_detect.jpg";
        for (int i = 4; i < argc - 1; i++) {
            if (std::string(argv[i]) == "-o") out_path = argv[i + 1];
        }
        cv::imwrite(out_path, img);
        printf("Saved: %s\n", out_path.c_str());

        delete g_scrfd_engine;
        return 0;
    }

    // ── Register command ──
    if (cmd == "register") {
        if (argc < 7) { printUsage(argv[0]); return 1; }

        const char* scrfd_model = argv[2];
        const char* facenet_model = argv[3];
        const char* photo = argv[4];
        const char* name = argv[5];
        const char* db_path = argv[6];

        // Load models
        g_scrfd_engine = new ma::engine::EngineCVI();
        g_scrfd_engine->init();
        g_scrfd_engine->load(scrfd_model);

        g_emb_engine = new ma::engine::EngineCVI();
        g_emb_engine->init();
        g_emb_engine->load(facenet_model);

        // Load existing DB
        g_facedb.load(db_path);

        // Read photo
        cv::Mat img = cv::imread(photo);
        if (img.empty()) { printf("Failed to read: %s\n", photo); return 1; }

        // Detect face
        auto faces = detectFaces(img);
        if (faces.empty()) {
            printf("No face detected!\n");
            return 1;
        }

        // Take the largest face
        auto& face = faces[0];

        // Align face to 112x112
        float landmarks[5][2];
        for (int i = 0; i < 5; i++) {
            landmarks[i][0] = face.landmarks[i].x;
            landmarks[i][1] = face.landmarks[i].y;
        }

        // Convert to BGR planar for aligner
        std::vector<uint8_t> bgr_planar(img.rows * img.cols * 3);
        for (int y = 0; y < img.rows; y++) {
            for (int x = 0; x < img.cols; x++) {
                int idx = y * img.cols + x;
                bgr_planar[idx] = img.at<cv::Vec3b>(y, x)[0];                  // B
                bgr_planar[img.rows * img.cols + idx] = img.at<cv::Vec3b>(y, x)[1];  // G
                bgr_planar[2 * img.rows * img.cols + idx] = img.at<cv::Vec3b>(y, x)[2]; // R
            }
        }

        float M[6];
        FaceAligner::computeTransform(landmarks, M);

        uint8_t aligned[112 * 112 * 3];
        FaceAligner::align(bgr_planar.data(), img.rows, img.cols, M, aligned);

        // Extract embedding
        auto emb = extractEmbedding(aligned);

        // Add to database
        g_facedb.addFace(name, emb);
        g_facedb.save(db_path);
        printf("Registered: %s\n", name);

        delete g_scrfd_engine;
        delete g_emb_engine;
        return 0;
    }

    // ── Identify command ──
    if (cmd == "identify") {
        if (argc < 6) { printUsage(argv[0]); return 1; }

        const char* scrfd_model = argv[2];
        const char* facenet_model = argv[3];
        const char* photo = argv[4];
        const char* db_path = argv[5];

        // Load models
        g_scrfd_engine = new ma::engine::EngineCVI();
        g_scrfd_engine->init();
        g_scrfd_engine->load(scrfd_model);

        g_emb_engine = new ma::engine::EngineCVI();
        g_emb_engine->init();
        g_emb_engine->load(facenet_model);

        // Load face database
        g_facedb.load(db_path);
        if (g_facedb.size() == 0) {
            printf("Face database is empty. Register faces first.\n");
            return 1;
        }

        // Read photo
        cv::Mat img = cv::imread(photo);
        if (img.empty()) { printf("Failed to read: %s\n", photo); return 1; }

        // Detect faces
        auto faces = detectFaces(img);
        printf("Detected %zu faces\n", faces.size());

        // Prepare BGR planar for alignment
        std::vector<uint8_t> bgr_planar(img.rows * img.cols * 3);
        for (int y = 0; y < img.rows; y++) {
            for (int x = 0; x < img.cols; x++) {
                int idx = y * img.cols + x;
                bgr_planar[idx] = img.at<cv::Vec3b>(y, x)[0];
                bgr_planar[img.rows * img.cols + idx] = img.at<cv::Vec3b>(y, x)[1];
                bgr_planar[2 * img.rows * img.cols + idx] = img.at<cv::Vec3b>(y, x)[2];
            }
        }

        // Process each face
        std::vector<std::string> names(faces.size());
        std::vector<float> scores(faces.size());

        for (size_t fi = 0; fi < faces.size(); fi++) {
            auto& face = faces[fi];

            float landmarks[5][2];
            for (int i = 0; i < 5; i++) {
                landmarks[i][0] = face.landmarks[i].x;
                landmarks[i][1] = face.landmarks[i].y;
            }

            float M[6];
            FaceAligner::computeTransform(landmarks, M);

            uint8_t aligned[112 * 112 * 3];
            FaceAligner::align(bgr_planar.data(), img.rows, img.cols, M, aligned);

            auto emb = extractEmbedding(aligned);
            auto [name, score] = g_facedb.match(emb);

            names[fi] = name;
            scores[fi] = score;

            if (name.empty()) {
                printf("Face %zu: unknown (best score: %.3f)\n", fi, score);
            } else {
                printf("Face %zu: %s (%.3f)\n", fi, name.c_str(), score);
            }
        }

        // Draw results
        for (size_t fi = 0; fi < faces.size(); fi++) {
            auto& f = faces[fi];
            cv::rectangle(img, cv::Point(f.x1, f.y1), cv::Point(f.x2, f.y2), cv::Scalar(0, 255, 0), 2);
            for (int i = 0; i < 5; i++) {
                cv::circle(img, cv::Point(f.landmarks[i].x, f.landmarks[i].y), 2, cv::Scalar(0, 0, 255), -1);
            }
            char label[128];
            if (!names[fi].empty()) {
                snprintf(label, sizeof(label), "%s %.2f", names[fi].c_str(), scores[fi]);
            } else {
                snprintf(label, sizeof(label), "unknown %.2f", scores[fi]);
            }
            cv::putText(img, label, cv::Point(f.x1, f.y1 - 8),
                        cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 0), 2);
        }

        // Save result
        std::string out_path = "result_identify.jpg";
        for (int i = 6; i < argc - 1; i++) {
            if (std::string(argv[i]) == "-o") out_path = argv[i + 1];
        }
        cv::imwrite(out_path, img);
        printf("Saved: %s\n", out_path.c_str());

        delete g_scrfd_engine;
        delete g_emb_engine;
        return 0;
    }

    printUsage(argv[0]);
    return 1;
}
