#ifndef DETECT_HPP
#define DETECT_HPP

#include <vector>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

enum class LightType { OUTPOST, BASE };

struct Light
{
    cv::Point2f center;
    float       radius;
    float       confidence;
    LightType   type = LightType::OUTPOST;
};

/* 函数声明 */
void classifyLights(std::vector<Light>& lights, int imgRows);

class GreenLightDetector
{
public:
    GreenLightDetector();
    std::vector<Light> detect(const cv::Mat& bgr);
    void               drawResult(cv::Mat& bgr, const std::vector<Light>& lights);

private:
    /* 各种参数与成员 */
    cv::Scalar hsvLow_;
    cv::Scalar hsvHigh_;
    int        diffTh_;
    int   minArea_;
    float minRadius_;
    float maxRadius_;
    float minConf_;

    int   minAreaFar_;
    float minRadiusFar_;
    float minConfFar_;
    int   blobMinThresh_;

    cv::Mat kernelSmall_;
    cv::Mat kernelLarge_;
    cv::Ptr<cv::CLAHE> clahe_;

    cv::Mat                  prevGray_;
    std::vector<cv::Point2f> prevPts_;
};

#endif // DETECT_HPP