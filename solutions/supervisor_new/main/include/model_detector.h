#ifndef MODEL_DETECTOR_H
#define MODEL_DETECTOR_H
// 头文件内容
#endif

#include <opencv2/opencv.hpp>
#include <sscma.h>
#include <numeric>

class ColorPalette {
public:
    static std::vector<cv::Scalar> getPalette();
    static cv::Scalar getColor(int index);

private:
    static const std::vector<cv::Scalar> palette;
};

// 图像预处理函数声明
cv::Mat preprocessImage(cv::Mat& image, ma::Model* model);

// 主检测函数声明
std::string model_detector(const std::string& model_path);