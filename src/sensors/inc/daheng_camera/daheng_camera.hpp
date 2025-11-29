#pragma once

#ifndef DAHENG_CAMERA_HPP_
#define DAHENG_CAMERA_HPP_

#include "sensors.hpp"
#include <opencv2/opencv.hpp>

#include "daheng_camera/daheng/GxIAPI.h"
#include "daheng_camera/daheng/GXDef.h"
#include "daheng_camera/daheng/DxImageProc.h"
#include "daheng_camera/daheng/GXErrorList.h"
#include "daheng_camera/daheng/GxPixelFormat.h"
#include "daheng_camera/daheng/GxIAPILegacy.h"

#include <thread>
#include <chrono>
#include <string>


class DahengCamera : public virtual sensors::BaseCamera   // 类名保持不变，直接替换
{
public:
    explicit DahengCamera(SerialPort &sp);
    ~DahengCamera();

    /* 生命周期 */
    void cameraInit() override;

    enum Camera_State
{
    IDLE,
    OPENED,
    GRABBING,
    WRONG_STATE
};

private:
    /* 线程主体：取图 + 解码 + 投递 */
    void getImage() override;
    /* 根据装甲板颜色动态调参 */
    void cameraModeChange(int now_mode, int my_color) override;
    /* SDK 返回值检查 */
    void assertSuccess(uint32_t status) override;

    /* —————— 内部工具 —————— */
    bool createHandle();
    bool openCamera();
    void closeCamera();
    bool grapImage();

    /* —————— 参数读写（保持原签名，仅内部换成大恒 API） —————— */
    uint64_t        getIntValue   (GX_FEATURE_ID_CMD key);
    float           getFloatValue (GX_FEATURE_ID_CMD key);
    uint64_t        getEnumValue  (GX_FEATURE_ID_CMD key);
    bool            getBoolValue  (GX_FEATURE_ID_CMD key);
    std::string getStringValue(GX_FEATURE_ID_CMD key);

    void setIntValue   (GX_FEATURE_ID_CMD key, int64_t  value);
    void setFloatValue (GX_FEATURE_ID_CMD key, double   value);
    void setEnumValue  (GX_FEATURE_ID_CMD key, int64_t value);
    void setBoolValue  (GX_FEATURE_ID_CMD key, bool     value);
    void setStringValue(const char *key, const char *value);

    /* 触发模式与 ROI */
    void setTriggerMode();
    void setResolution(int max_width,
                       int roi_width,
                       int roi_width_offset,
                       int max_height,
                       int roi_height,
                       int roi_height_offset);

    /* —————— 成员变量 —————— */
    GX_DEV_HANDLE handle_ = nullptr;      // 替换 MV_CC_HANDLE

    uint32_t      camera_size_ = 0;       // 已打开相机数量
    uint64_t      nDataSize_   = 0;       // 图像 payload
    unsigned char *pBGR_       = nullptr; // 转码后 BGR 缓存

    int           camera_state_ = Camera_State::IDLE;
    foxglove::Time error_;                // 上次出错时间戳

    /* 缓存上一帧模式/颜色，避免重复设参 */
    int pre_mode_     = -1;
    int pre_my_color_ = -1;
};

#endif // DAHENG_CAMERA_HPP_