#ifndef __ARMOR_DETECT_HPP__
#define __ARMOR_DETECT_HPP__

#include <opencv2/opencv.hpp>

#include "lockfree/LockFree.hpp"
#include "openvino/OpenvinoInfer.hpp"
#include "global/Struct.hpp"
#include "traditional/traditional_detect.hpp"

class Detect {
public:
    explicit Detect(const char *mode);
    Detect() = delete;
    ~Detect();

    void detect(
            LfStack<FromData> &message_stack,
            LfStack<ImageInfo> &image_info_stack,
            LfStack <ImageInfo> &mouse_info_stack);

    bool useTraditionalDetector() const; //是否使用传统检测方案

private:
    OpenvinoInfer *openvino_infer_;
    GreenLightDetector *traditional_detector_; //传统检测方案

    FromData last_message_;//上一帧的信息

    std::deque<double> time_minus_queue_;//时间差队列

    bool startup_ = true;//是否是启动状态

    std::string mode_;//不同检测方案

    bool useTraditionalDetection() const ; //是否使用传统检测方案

};


#endif // __ARMOR_DETECT_HPP__