#include "daheng_camera/daheng_camera.hpp"

#include <opencv2/opencv.hpp>
#include <thread>

#include "function/function.hpp"

DahengCamera::DahengCamera(SerialPort &sp) : sensors::BaseCamera(sp) {
    this->error_ = function::getNowTimestamp();
}

DahengCamera::~DahengCamera() {
    this->closeCamera();
}

void DahengCamera::cameraInit() {
    std::cout<< "start camera"<<std::endl;

    GX_STATUS status = GX_STATUS_SUCCESS;
    this->camera_size_ = 0;

    status = GXInitLib();
    if (status != GX_STATUS_SUCCESS) {
        printf("GXInitLib fail for status = [%x].\n", status);
        return;
    }

    std::cout<< "start camera1"<<std::endl;

    uint32_t n_dev_ = 0;
    status = GXUpdateDeviceList(&n_dev_, 1000);
    if (status != GX_STATUS_SUCCESS || n_dev_ == 0) {
        printf("No Devices Found.\n");
        return;
    }

    if (n_dev_ > 0) {
        for (size_t i = 0 ; i < n_dev_ ; ++i) {
            std::string indexStr = std::to_string(i);
            GX_OPEN_PARAM stOpenParam{};
            stOpenParam.accessMode = GX_ACCESS_EXCLUSIVE;
            stOpenParam.openMode   = GX_OPEN_INDEX;
            stOpenParam.pszContent = (char*)indexStr.c_str();

            if (!this->createHandle()) {
                continue;
            }

            if (!this->openCamera()) {
                continue;
            }

            std::cout<< "opening camera";

            this->setEnumValue(GX_ENUM_TRIGGER_MODE, GX_TRIGGER_MODE_OFF);
            this->setEnumValue(GX_ENUM_PIXEL_FORMAT, GX_PIXEL_FORMAT_BAYER_RG8);
            this->setBoolValue(GX_ENUM_ACQUISITION_FRAME_RATE_MODE, true);
            this->setFloatValue(GX_FLOAT_ACQUISITION_FRAME_RATE, 250.0);
            this->setResolution(-1, 384, 300, -1, 640, 0);
            if (!this->grapImage()) {
                return;
            }
            setFloatValue(GX_FLOAT_EXPOSURE_TIME, 3000);
            setFloatValue(GX_FLOAT_GAIN, 8.0);
            ++this->camera_size_;
        }
    } else {
        printf("No Devices Found.\n");
        return;
    }

    printf("Initialize %d camera(s).\n", this->camera_size_);
}
bool DahengCamera::createHandle()
{
    GX_OPEN_PARAM stOpenParam{};                                   //储存打开相机的参数
    stOpenParam.accessMode = GX_ACCESS_EXCLUSIVE;                  //设置访问模式为独占访问
    stOpenParam.openMode   = GX_OPEN_INDEX;                        //设置打开模式为根据索引打开
    
    // 修复悬空指针问题：将索引字符串存储在持久化变量中
    std::string indexStr = std::to_string(0);
    stOpenParam.pszContent = (char*)indexStr.c_str();              //设置打开参数为相机索引

    GX_STATUS status = GXOpenDevice(&stOpenParam, &handle_);
    printf("GXOpenDevice returned status = 0x%x, handle_ = %p\n", status, handle_);
    
    return status == GX_STATUS_SUCCESS;
}
bool DahengCamera::openCamera() {
    if (this->handle_ == nullptr) {
        return false;
    }

    return true;
}

void DahengCamera::closeCamera() {
    GX_STATUS status = GXStreamOff(this->handle_);
    if (status != GX_STATUS_SUCCESS) {
        printf("GXStreamOff fail for status = [%x].\n", status);
    }

    status = GXCloseDevice(this->handle_);
    if (status != GX_STATUS_SUCCESS) {
        printf("GXCloseDevice fail for status = [%x].\n", status);
    }

    status = GXCloseLib();
    if (status != GX_STATUS_SUCCESS) {
        printf("GXCloseLib fail for status = [%x].\n", status);
    }
}

uint64_t DahengCamera::getIntValue(GX_FEATURE_ID_CMD key) {
    int64_t value;
    GX_STATUS status = GXGetInt(this->handle_, key, &value);

    if (status != GX_STATUS_SUCCESS) {
        printf("GXGetInt fail for status = [%x].\n", status);
        return 0;
    }

    return value;
}

float DahengCamera::getFloatValue(GX_FEATURE_ID_CMD key) {
    double value;
    GX_STATUS status = GXGetFloat(this->handle_, key, &value);

    if (status != GX_STATUS_SUCCESS) {
        printf("GXGetFloat fail for status = [%x].\n", status);
        return 0;
    }

    return value;
}

uint64_t DahengCamera::getEnumValue(GX_FEATURE_ID_CMD key) {
    int64_t value;
    GX_STATUS status = GXGetEnum(this->handle_, key, &value);

    if (status != GX_STATUS_SUCCESS) {
        printf("GXGetEnum fail for status = [%x].\n", status);
        return 0;
    }

    return value;
}

bool DahengCamera::getBoolValue(GX_FEATURE_ID_CMD key) {
    bool value;
    GX_STATUS status = GXGetBool(this->handle_, key, &value);

    if (status != GX_STATUS_SUCCESS) {
        printf("GXGetBool fail for status = [%x].\n", status);
        return false;
    }

    return value;
}

void DahengCamera::setTriggerMode() {
    GX_STATUS status = GXSetEnum(this->handle_, GX_ENUM_TRIGGER_MODE, GX_TRIGGER_MODE_OFF);

    if (status != GX_STATUS_SUCCESS) {
        printf("GXSetEnum fail for status = [%x].\n", status);
    }
}

void DahengCamera::setIntValue(GX_FEATURE_ID_CMD key, int64_t value) {
    GX_STATUS status = GXSetInt(this->handle_, key, value);

    if (status != GX_STATUS_SUCCESS) {
        printf("GXSetInt fail for status = [%x].\n", status);
    }
}

void DahengCamera::setFloatValue(GX_FEATURE_ID_CMD key, double value) {
    GX_STATUS status = GXSetFloat(this->handle_, key, value);

    if (status != GX_STATUS_SUCCESS) {
        printf("GXSetFloat fail for status = [%x].\n", status);
    }
}

void DahengCamera::setEnumValue(GX_FEATURE_ID_CMD key, int64_t value) {
    GX_STATUS status = GXSetEnum(this->handle_, key, value);

    if (status != GX_STATUS_SUCCESS) {
        printf("GXSetEnum failed for key [%lld], value [%lld], error code [%x].\n", key, value, status);
    } else {
        printf("GXSetEnum succeeded for key [%lld], value [%lld].\n", key, value);
    }
}

void DahengCamera::setBoolValue(GX_FEATURE_ID_CMD key, bool value) {
    GX_STATUS status = GXSetBool(this->handle_, key, value);

    if (status != GX_STATUS_SUCCESS) {
        printf("GXSetBool fail for status = [%x].\n", status);
    }
}

void DahengCamera::setResolution(
        int max_width,
        int roi_width,
        int roi_width_offset,
        int max_height,
        int roi_height,
        int roi_height_offset) {
    GX_STATUS status = GX_STATUS_SUCCESS;
    if (this->camera_state_ >= Camera_State::GRABBING) {
        printf("Stop grabbing first.\n");

        status = GXStreamOff(this->handle_);

        // 只在有图形界面时使用waitKey
        if (getenv("DISPLAY")) {
            cv::waitKey(200);
        } else {
            // 无图形界面时，使用标准休眠
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }

    if (status != GX_STATUS_SUCCESS) {
        printf("GXStreamOff fail for status = [%x].\n", status);
        return;
    }

    this->setIntValue(GX_INT_WIDTH, roi_width);
    this->setIntValue(GX_INT_OFFSET_X, roi_width_offset);

    this->setIntValue(GX_INT_HEIGHT, roi_height);
    this->setIntValue(GX_INT_OFFSET_Y, roi_height_offset);

    if (this->camera_state_ >= Camera_State::GRABBING) {
        this->grapImage();
    }
}

bool DahengCamera::grapImage() {
    GX_STATUS status = GXStreamOn(this->handle_);
    if (status != GX_STATUS_SUCCESS) {
        printf("GXStreamOn fail for status = [%x].\n", status);
        return false;
    }

    this->nDataSize_ = this->getIntValue(GX_INT_PAYLOAD_SIZE);

    this->camera_state_ = Camera_State::GRABBING;

    return true;
}

void DahengCamera::cameraModeChange(int now_mode, int my_color) {
    if ((now_mode == 1 || now_mode == 0) && (now_mode != this->pre_mode_ || my_color != this->pre_my_color_)) {
        if (my_color == 0) {
            std::cout << "Detect Red." << std::endl;
            float val = J_SENSORS.config_["camera"]["daheng"]["exposure"]["red_exposure"];
            std::cout << "val = " << val << std::endl;
            this->setFloatValue(GX_FLOAT_EXPOSURE_TIME, val);
        } else if (my_color == 1) {
            std::cout << "Detect Blue." << std::endl;
            float val = J_SENSORS.config_["camera"]["daheng"]["exposure"]["blue_exposure"];
            std::cout << "val = " << val << std::endl;
            this->setFloatValue(GX_FLOAT_EXPOSURE_TIME, val);
        }
    }
    this->pre_mode_ = now_mode;
    this->pre_my_color_ = my_color;
}

void DahengCamera::assertSuccess(uint32_t status) {
    if (status != GX_STATUS_SUCCESS) {
        printf("Camera_Error: %x\n", status);
        this->closeCamera();
        return;
    }
}

void DahengCamera::getImage() {

    std::cout<< "start"<<std::endl;

    CarData car_data;

    while(!this->sp_.receive(car_data)) {
        std::cout<< "don't have data"<<std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    int my_color = 0;
    this->cameraModeChange(car_data.mode, my_color);
    std::cout<< "have data"<<std::endl;
    this->message_.cd = car_data;
    this->message_.timestamp = function::getNowTimestamp();
    this->error_ = function::getNowTimestamp();
    memset(&this->stOutFrame_, 0, sizeof(GX_FRAME_DATA));
    
    while(GXGetImage(this->handle_, &this->stOutFrame_, 1000) != GX_STATUS_SUCCESS) {
        this->message_.timestamp = function::getNowTimestamp();
        auto minus = function::timestampMinus(this->message_.timestamp, this->error_) / 1e3;
        if (minus > (double)J_SENSORS.config_["camera"]["drop_time"]) {
            exit(0);
        }
    }

    if (this->stOutFrame_.nPixelFormat == GX_PIXEL_FORMAT_BAYER_RG8 && this->stOutFrame_.pImgBuf != nullptr) {

        delete[] this->pBGR_;
        this->pBGR_ = new unsigned char[sizeof(unsigned char) * this->stOutFrame_.nWidth * this->stOutFrame_.nHeight * 3 + 2048];

        DxRaw8toRGB24(this->stOutFrame_.pImgBuf, this->pBGR_, 
                      this->stOutFrame_.nWidth, this->stOutFrame_.nHeight, 
                      RAW2RGB_NEIGHBOUR3, DX_PIXEL_COLOR_FILTER(BAYERRG), false);

        this->message_.img = cv::Mat(this->stOutFrame_.nHeight, this->stOutFrame_.nWidth, CV_8UC3, this->pBGR_).clone();
        cv::transpose(this->message_.img, this->message_.img);
        cv::flip(this->message_.img, this->message_.img, -1);
        cv::flip(this->message_.img, this->message_.img, 1);

        //cv::imshow("img", this->message_.img);
        //cv::waitKey(1);
    } else {
        printf("Unsupported PixelType.\n");
        return;
    }
}