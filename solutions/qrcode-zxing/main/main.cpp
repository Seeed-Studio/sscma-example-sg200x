#include <iostream>
#include <opencv2/opencv.hpp>
#include <zxing/Binarizer.h>
#include <zxing/BinaryBitmap.h>
#include <zxing/DecodeHints.h>
#include <zxing/MatSource.h>
#include <zxing/MultiFormatReader.h>
#include <zxing/Result.h>
#include <zxing/common/HybridBinarizer.h>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <image_file>" << std::endl;
        return 1;
    }

    // Load image using OpenCV in grayscale mode
    cv::Mat image = cv::imread(argv[1], cv::IMREAD_GRAYSCALE);
    if (image.empty()) {
        std::cerr << "Error: Could not load image " << argv[1] << std::endl;
        return 1;
    }

    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Create a luminance source from OpenCV Mat
    zxing::Ref<zxing::LuminanceSource> source = MatSource::create(image);

    // Create a binarizer to convert luminance source to binary bitmap
    zxing::Ref<zxing::Binarizer> binarizer(new zxing::HybridBinarizer(source));

    // Create binary bitmap from binarizer
    zxing::Ref<zxing::BinaryBitmap> bitmap(new zxing::BinaryBitmap(binarizer));

    // Setup hints for decoding (enable try harder mode)
    zxing::DecodeHints hints(zxing::DecodeHints::DEFAULT_HINT);

    // Create multi-format reader
    zxing::MultiFormatReader reader;

    // Decode the barcode
    zxing::Ref<zxing::Result> result = reader.decode(bitmap, hints);

     // End timing
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end_time - start_time;

    // Output results
    std::cout << "Barcode format: " << result->getBarcodeFormat() << std::endl;
    std::cout << "Barcode text: " << result->getText()->getText() << std::endl;
    std::cout << "Decoding time: " << elapsed.count() << " ms" << std::endl;


    return 0;
}
