// Include necessary libraries
#include <quirc.h>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <chrono>
#include <vector>

// Main function
int main(int argc, char* argv[]) {
    // Check command line arguments
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <image_file>" << std::endl;
        return 1;
    }

    // Load image in grayscale
    cv::Mat image = cv::imread(argv[1], cv::IMREAD_GRAYSCALE);
    if (image.empty()) {
        std::cerr << "Error: Could not load image " << argv[1] << std::endl;
        return 1;
    }

    // Resize image to reduce processing time (optional, adjust as needed)
    cv::Mat resized;
    cv::resize(image, resized, cv::Size(640, 640), 0, 0, cv::INTER_AREA);

    // Enhance contrast using histogram equalization (optional)
    cv::Mat enhanced;
    cv::equalizeHist(resized, enhanced);

    // Number of decoding iterations for average time calculation
    const int iterations = 10;
    std::vector<double> decode_times;

    // Perform multiple decoding iterations
    for (int i = 0; i < iterations; ++i) {
        auto start_time = std::chrono::high_resolution_clock::now();

        // Initialize Quirc
        struct quirc* qr = quirc_new();
        if (!qr) {
            std::cerr << "Error: Failed to initialize Quirc" << std::endl;
            return 1;
        }

        // Resize Quirc buffer to match image dimensions
        if (quirc_resize(qr, enhanced.cols, enhanced.rows) < 0) {
            std::cerr << "Error: Failed to resize Quirc buffer" << std::endl;
            quirc_destroy(qr);
            return 1;
        }

        // Copy image data to Quirc buffer
        uint8_t* buffer = quirc_begin(qr, nullptr, nullptr);
        memcpy(buffer, enhanced.data, enhanced.cols * enhanced.rows);
        quirc_end(qr);

        // Extract and decode QR codes
        int count = quirc_count(qr);
        for (int j = 0; j < count; j++) {
            struct quirc_code code;
            struct quirc_data data;
            quirc_extract(qr, j, &code);
            if (quirc_decode(&code, &data) == QUIRC_SUCCESS) {
                // Output QR code content only for the first iteration
                if (i == 0) {
                    std::cout << "QR Code: " << data.payload << std::endl;
                }
            }
        }

        // Record end time and calculate duration
        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end_time - start_time;
        decode_times.push_back(elapsed.count());

        // Clean up Quirc instance
        quirc_destroy(qr);
    }

    // Calculate and output average decoding time
    double total_time = 0.0;
    for (double time : decode_times) {
        total_time += time;
    }
    double average_time = total_time / iterations;
    std::cout << "Number of iterations: " << iterations << std::endl;
    std::cout << "Average decoding time: " << average_time << " ms" << std::endl;

    // Output individual decoding times
    std::cout << "Individual decoding times (ms): ";
    for (size_t i = 0; i < decode_times.size(); ++i) {
        std::cout << decode_times[i];
        if (i < decode_times.size() - 1) std::cout << ", ";
    }
    std::cout << std::endl;

    return 0;
}