#include "daheng_camera/daheng_camera.hpp"
#include <daheng_camera/daheng/DxImageProc.h>
#include <daheng_camera/daheng/GxIAPI.h>
#include <cstring>
#include <iostream>
// 如果 function 在全局命名空间，用 ::function；如有头文件请包含
#include "function/function.hpp" // 根据你工程实际路径调整

/* 工具 */
static inline uint32_t toUint(GX_STATUS s) { return static_cast<uint32_t>(s); }

DahengCamera::DahengCamera(SerialPort &sp) : BaseCamera(sp)
{
    this->error_ = function::getNowTimestamp();
}

DahengCamera::~DahengCamera()
{
    this->closeCamera();
}

void DahengCamera::cameraInit()
{ // 初始化相机
    std::cout << "start camera\n"
              << std::endl;

    GX_STATUS status = GX_STATUS_SUCCESS;
    if (GXInitLib() != GX_STATUS_SUCCESS)
    {
        printf("GXInitLib fail\n");
        return;
    }

    this->camera_size_ = 0;
    // memset(this->stDeviceInfo, 0 ,sizeof(GX_DEVICE_BASE_INFO));

    uint32_t n_dev_ = 0;
    if (GXUpdateDeviceList(&n_dev_, 1000) != GX_STATUS_SUCCESS || n_dev_ == 0)
    {
        printf("No Devices Found.\n");
        return ;
    }

    std::vector<GX_DEVICE_BASE_INFO> devInfo(n_dev_);
    size_t bufSize = n_dev_ * sizeof(GX_DEVICE_BASE_INFO);
    if (GXGetAllDeviceBaseInfo(devInfo.data(), &bufSize) != GX_STATUS_SUCCESS)
    {
        printf("GXGetAllDeviceBaseInfo fail\n");
        return;
    }

    if (n_dev_ > 0)
    {
        // printf("nDev = %d\n", nDev);
        for (size_t i = 0; i < n_dev_; ++i)
        {
            if (!createHandle())
            {
                continue;
            }

            if (!openCamera())
            {
                continue;
            }

            std::cout << "opening the camera!!" << std::endl;

            this->setEnumValue( GX_ENUM_TRIGGER_MODE, GX_TRIGGER_MODE_OFF);       //关闭触发模式
            this->setEnumValue( GX_ENUM_PIXEL_FORMAT, GX_PIXEL_FORMAT_BAYER_RG8); //设置像素格式为 Bayer RG8
            this->setBoolValue( GX_ENUM_ACQUISITION_FRAME_RATE_MODE, true);       //使能帧率模式
            this->setFloatValue(GX_FLOAT_ACQUISITION_FRAME_RATE, 250.0);         //设置帧率为 250 fps

            this->setResolution(-1, 384, 300, -1, 640, 0);
            if (!grapImage())
                return;

            this->setFloatValue(GX_FLOAT_EXPOSURE_TIME, 3000);
            this->setFloatValue(GX_FLOAT_GAIN, 8.0);
            camera_size_ = 1;
            printf("Initialize %d camera(s).\n", camera_size_);
        }
    }
}

bool DahengCamera::createHandle()
{
    GX_OPEN_PARAM stOpenParam{};                                   //储存打开相机的参数
    stOpenParam.accessMode = GX_ACCESS_EXCLUSIVE;                  //设置访问模式为独占访问
    stOpenParam.openMode   = GX_OPEN_INDEX;                        //设置打开模式为根据索引打开
    stOpenParam.pszContent = (char*)std::to_string(0).c_str();     //设置打开参数为相机索引

    return GXOpenDevice(&stOpenParam, &handle_) == GX_STATUS_SUCCESS;
}

bool DahengCamera::openCamera()
{
    if (this->handle_ == nullptr)
    {
        printf("fail to open the camera\n");
        return false;
    }

    return true;
}

void DahengCamera::closeCamera()
{
    if (!handle_)
        return;

    auto n_ret = GXStreamOff(this->handle_);
    if (n_ret != GX_STATUS_SUCCESS)
    {
        printf("GXStreamOff fail for n_ret = [%x].\n", n_ret);
    }

    n_ret = GXCloseDevice(this->handle_);
    if (n_ret != GX_STATUS_SUCCESS)
    {
        printf("GXCloseDevice fail for n_ret = [%x].\n", n_ret);
    }

    handle_ = nullptr;

    n_ret = GXCloseLib();
    if (n_ret != GX_STATUS_SUCCESS)
    {
        printf("GXCloseLib fail for n_ret = [%x].\n", n_ret);
    }
}

/* 参数读写 */
uint64_t DahengCamera::getIntValue(GX_FEATURE_ID_CMD key){
        int64_t value;
        GX_STATUS status = GXGetInt(handle_, key, &value); // 调用大恒 SDK 函数获取整数值
      
        if (status != GX_STATUS_SUCCESS) {
            // 若操作失败，输出错误信息
            printf("GXGetInt fail for status = [%x].\n", status);
            return 0;
        }

        return static_cast<int64_t>(value); 
    }

float DahengCamera::getFloatValue(GX_FEATURE_ID_CMD key) {
        double value;
        GX_STATUS status = GXGetFloat(handle_, key, &value); // 调用大恒 SDK 函数获取浮点值

        if (status != GX_STATUS_SUCCESS) {
            // 若操作失败，输出错误信息
            printf("GXGetFloat fail for status = [%x].\n", status);
            return 0;
        }

        return static_cast<float>(value);
    }


uint64_t DahengCamera::getEnumValue(GX_FEATURE_ID_CMD key){
        int64_t value;
        GX_STATUS status = GXGetEnum(handle_, key, &value); // 调用大恒 SDK 函数获取枚举值

        if (status != GX_STATUS_SUCCESS) {
            // 若操作失败，输出错误信息
            printf("GXGetEnum fail for status = [%x].\n", status);
            return 0;
        }

        return value;
    }

bool DahengCamera::getBoolValue(GX_FEATURE_ID_CMD key){
        bool value;
        GX_STATUS status = GXGetBool(handle_, key, &value); // 调用大恒 SDK 函数获取布尔值

        if (status != GX_STATUS_SUCCESS) {
            // 若操作失败，输出错误信息
            printf("GXGetBool fail for status = [%x].\n", status);
            return false;
        }

        return value;
    }

std::string DahengCamera::getStringValue(GX_FEATURE_ID_CMD key) {
        char buffer[1024] = {0};
        size_t size = sizeof(buffer);
        GX_STATUS status = GXGetString(handle_, key, buffer, &size);

        if (status != GX_STATUS_SUCCESS) {
            // 若操作失败，输出错误信息
            printf("GXGetString fail for status = [%x].\n", status);
        }

        return std::string(buffer);
    }


void DahengCamera::setIntValue(GX_FEATURE_ID_CMD key, int64_t v)
{
    GX_STATUS status = GX_STATUS_SUCCESS;
    status = GXSetInt(this->handle_, key, v);
    if (status != GX_STATUS_SUCCESS){
        printf("GXSetInt fail for status = [%x].\n", status);
    }
}

void DahengCamera::setFloatValue(GX_FEATURE_ID_CMD key, double v)
{
    GX_STATUS status = GXSetFloat(handle_, key, v);
    if (status != GX_STATUS_SUCCESS)
        printf("GXSetFloat fail: %x\n", status);
}

void DahengCamera::setEnumValue(GX_FEATURE_ID_CMD key, int64_t value)
{
    GX_STATUS status = GXSetEnum(handle_, key, value);
    if (status != GX_STATUS_SUCCESS)
        printf("GXSetEnum failed, key=0x%x, val=%lu, err=0x%x\n", key, value, status);
}


void DahengCamera::setBoolValue(GX_FEATURE_ID_CMD key, bool value) {
        GX_STATUS status = GX_STATUS_SUCCESS;
        status = GXSetBool(this->handle_, key, value);

        if (status != GX_STATUS_SUCCESS) {
            // 若操作失败，输出错误信息
            printf("GXSetBool failed for key [0x%llx], value [%d], error code [0x%x].\n", 
                   static_cast<unsigned long long>(key), 
                   value, 
                   status);
        } else {
            // 若操作成功，输出成功信息
            printf("GXSetBool succeeded for key [0x%llx], value [%d].\n", 
                   static_cast<unsigned long long>(key), 
                   value);
        }
    }

void DahengCamera::setTriggerMode()
{
    setEnumValue(GX_ENUM_TRIGGER_MODE, 0);  

    // 如果你想额外打印，可以这样做：
    std::cout << "Trigger mode set to OFF." << std::endl;
}

void DahengCamera::setResolution(
    int max_wideth,
    int roi_wideth,
    int roi_wideth_offset,
    int max_height,
    int roi_height,
    int roi_height_offset)
{ // 设置分辨率
    auto n_ret = GX_STATUS_SUCCESS;
    if (this->camera_state_ >= Camera_State::GRABBING)
    {
        printf("Stop grabbing first.\n");
        assertSuccess(toUint(GXStreamOff(handle_)));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    if (n_ret != GX_STATUS_SUCCESS)
    {
        std::cout << "Fail to set resolution." << std::endl;
    }

    this->setIntValue(GX_INT_WIDTH, roi_wideth);
    this->setIntValue(GX_INT_OFFSET_X, roi_wideth_offset);

    this->setIntValue(GX_INT_HEIGHT, roi_height);
    this->setIntValue(GX_INT_OFFSET_Y, roi_height_offset);

    if (camera_state_ >= Camera_State::GRABBING)
        grapImage();
}

bool DahengCamera::grapImage()
{ // 相机采集
    GX_STATUS status = GX_STATUS_SUCCESS;
    status = GXStreamOn(this->handle_);
    if (status != GX_STATUS_SUCCESS)
    {
        printf("GXStreamOn fail for status = [%x].\n", status);
        return false;
    }

    int64_t payloadsize = 0;
    
    status = GXGetInt(this->handle_, GX_INT_PAYLOAD_SIZE, &payloadsize);
    if (status != GX_STATUS_SUCCESS)
    {
        printf("GXGetInt fail for status = [%x].\n", status);
        GXStreamOff(this->handle_);
        return false;
    }

    this->nDataSize_ = static_cast<int64_t>(payloadsize);
    this->camera_state_ = Camera_State::GRABBING;

    return true;

}

void DahengCamera::cameraModeChange(int now_mode, int my_color) {
        if ((now_mode == 1 || now_mode == 0) && (now_mode != this->pre_mode_ || my_color != this->pre_my_color_)) {
            if (my_color == 0) {
                std::cout << "Detect Red." << std::endl;
                double val = J_DETECT.config_["camera"]["daheng"]["exposure"]["red_exposure"];
                std::cout << "val = " << val << std::endl;
                GX_STATUS status = GXSetFloat(this->handle_, GX_FLOAT_EXPOSURE_TIME, val);
                if (status != GX_STATUS_SUCCESS) {
                    printf("GXSetFloat fail for status = [%x].\n", status);
                }
            } else if (my_color == 1) {
                std::cout << "Detect Blue." << std::endl;
                double val = J_DETECT.config_["camera"]["daheng"]["exposure"]["blue_exposure"];
                std::cout << "val = " << val << std::endl;
                GX_STATUS status = GXSetFloat(this->handle_, GX_FLOAT_EXPOSURE_TIME, val);
                if (status != GX_STATUS_SUCCESS) {
                    printf("GXSetFloat fail for status = [%x].\n", status);
                }
            }
        }
        this->pre_mode_ = now_mode;
        this->pre_my_color_ = my_color;
    }

    void DahengCamera::assertSuccess(uint32_t status) {  
        if (status != GX_STATUS_SUCCESS) {
            printf("Camera_Error: %x\n", status);
            closeCamera();
        }
    }

void DahengCamera::getImage()
{
    CarData car_data;

    /* 1. 等待控制指令 */
    while (!this->sp_.receive(car_data)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    this->message_.cd        = car_data;

    //CarData car_data{};          // 假数据，直接往下走
    //this->message_.cd = car_data;

    printf("getData!\n");

    this->message_.timestamp = function::getNowTimestamp();
    this->error_             = function::getNowTimestamp();
   

    /* 2. 从相机取一帧（1000 ms 超时） */
    GX_FRAME_DATA frame;
    memset(&frame, 0, sizeof(GX_FRAME_DATA));
    GX_STATUS status = GXGetImage(this->handle_, &frame, 1000);
    printf("GXGetImage ret = 0x%x (0 = success)\n", status);

    while (status != GX_STATUS_SUCCESS) {
        this->message_.timestamp = function::getNowTimestamp();
        auto minus = function::timestampMinus(this->message_.timestamp, this->error_) / 1e3;
        if (minus > (double)J_SENSORS.config_["camera"]["drop_time"]) {
            exit(0);
        }
        status = GXGetImage(this->handle_, &frame, 1000);
    }

        /* 3. 像素格式检查与转换（BayerRG8 → BGR8） */
    if (frame.nPixelFormat == GX_PIXEL_FORMAT_BAYER_RG8 && frame.pImgBuf) {
        delete[] this->pBGR_;
        this->pBGR_ = new unsigned char[frame.nWidth * frame.nHeight * 3];

        // 手动 BayerRG8 → BGR8
        auto bayerRG8ToBGR = [](const uint8_t* bayer, uint8_t* bgr, int w, int h) {
            for (int y = 1; y < h - 1; ++y) {
                for (int x = 1; x < w - 1; ++x) {
                    int r = bayer[y * w + x];
                    int g = (bayer[(y - 1) * w + x] + bayer[(y + 1) * w + x] +
                             bayer[y * w + x - 1] + bayer[y * w + x + 1]) / 4;
                    int b = (bayer[(y - 1) * w + x - 1] + bayer[(y - 1) * w + x + 1] +
                             bayer[(y + 1) * w + x - 1] + bayer[(y + 1) * w + x + 1]) / 4;
                    int idx = (y * w + x) * 3;
                    bgr[idx + 0] = b;
                    bgr[idx + 1] = g;
                    bgr[idx + 2] = r;
                }
            }
        };

        bayerRG8ToBGR(static_cast<uint8_t*>(frame.pImgBuf), this->pBGR_, frame.nWidth, frame.nHeight);

        /* 4. 封装成 cv::Mat，并做转置/翻转 */
        this->message_.img = cv::Mat(frame.nHeight, frame.nWidth, CV_8UC3, this->pBGR_).clone();
        cv::transpose(this->message_.img, this->message_.img);
        cv::flip(this->message_.img, this->message_.img, -1);
        cv::flip(this->message_.img, this->message_.img, 1);

        /* 5. 显示（调试用） */
        cv::namedWindow("img", cv::WINDOW_AUTOSIZE);
        cv::imshow("img", this->message_.img);
        cv::waitKey(1);
    } else {
        printf("Unsupported PixelType.\n");
        return;
    }
}