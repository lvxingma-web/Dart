#ifndef __ARMOR_DETECT_HPP__
#define __ARMOR_DETECT_HPP__

#include <opencv2/opencv.hpp>

#include "lockfree/LockFree.hpp"
#include "openvino/OpenvinoInfer.hpp"
#include "global/Struct.hpp"

class Detect {
public:
    explicit Detect(const char *mode);
    Detect() = delete;
    ~Detect();

    void detect(
            LfStack<FromData> &message_stack,
            LfStack<ImageInfo> &image_info_stack,
            LfStack <ImageInfo> &mouse_info_stack);

private:
    OpenvinoInfer *openvino_infer_;

    FromData last_message_;//上一帧的信息

    std::deque<double> time_minus_queue_;//时间差队列

    bool startup_ = true;//是否是启动状态

    std::string mode_;//不同检测方案

};


#endif // __ARMOR_DETECT_HPP__