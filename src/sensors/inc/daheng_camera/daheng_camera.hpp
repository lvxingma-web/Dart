#ifndef DAHENG_CAMERA_HPP_
#define DAHENG_CAMERA_HPP_

#include "sensors.hpp"

#include "daheng_camera/daheng/GxIAPI.h"
#include "daheng_camera/daheng/GXDef.h"
#include "daheng_camera/daheng/DxImageProc.h"
#include "daheng_camera/daheng/GXErrorList.h"
#include "daheng_camera/daheng/GxPixelFormat.h"
#include "daheng_camera/daheng/GxIAPILegacy.h"

#ifndef CAMERA_STATE_DEFINED
#define CAMERA_STATE_DEFINED
enum Camera_State{
    IDLE,
    OPENED,
    GRABBING,
    WRONG_STATE
};
#endif // CAMERA_STATE_DEFINED

class DahengCamera : public virtual sensors::BaseCamera {
public:
    explicit DahengCamera(SerialPort &sp);
    ~DahengCamera();

    void cameraInit() override;

private:
    void getImage() override;
    void cameraModeChange(int now_mode, int my_color) override;
    void assertSuccess(uint32_t status) override;

    bool createHandle();
    bool openCamera();
    void closeCamera();

    uint64_t getIntValue(GX_FEATURE_ID_CMD key);
    float getFloatValue(GX_FEATURE_ID_CMD key);
    uint64_t getEnumValue(GX_FEATURE_ID_CMD key);
    bool getBoolValue(GX_FEATURE_ID_CMD key);

    void setTriggerMode();
    void setIntValue(GX_FEATURE_ID_CMD key, int64_t value);
    void setFloatValue(GX_FEATURE_ID_CMD key, double value);
    void setEnumValue(GX_FEATURE_ID_CMD key, int64_t value);
    void setBoolValue(GX_FEATURE_ID_CMD key, bool value);

    void setResolution(
            int max_width,
            int roi_width,
            int roi_width_offset,
            int max_height,
            int roi_height,
            int roi_height_offset);

    bool grapImage();

    GX_DEV_HANDLE handle_ = nullptr;

    uint32_t camera_size_ = 0;

    uint64_t nDataSize_ = 0;

    unsigned char* pBGR_ = nullptr;

    int camera_state_ = Camera_State::IDLE;

    GX_FRAME_DATA stOutFrame_;

    foxglove::Time error_;
};

#endif // DAHENG_CAMERA_HPP_