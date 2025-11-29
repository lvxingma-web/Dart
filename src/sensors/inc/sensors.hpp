#ifndef SENSOR_HPP_
#define SENSOR_HPP_

#include "lockfree/LockFree.hpp"

#include "serialport/SerialPort.h"

#include "global/Struct.hpp"

#include "global/Config.hpp"


namespace sensors {
class BaseCamera {
public:
    explicit BaseCamera(SerialPort &sp) : sp_(sp) {};
    ~BaseCamera() = default;

    virtual void cameraInit() = 0;

    template<class ...Args>
    void refreshImage(Args &&...args);//刷新相机图像
    
protected:
    template<typename T, class ...Args>
    void unpackArgs(T &&t, Args &&...args);

    void unpackArgs() {}//解包参数，递归终止函数

    virtual void cameraModeChange(int now_mode, int my_color) = 0;      //根据当前模式和我的颜色切换相机模式
    virtual void assertSuccess(uint32_t status) = 0;                    //检查操作状态是否成功
    virtual void getImage() = 0;
    FromData message_;
    SerialPort &sp_;
    int pre_my_color_ = -2;
    int pre_mode_ = -2;

private:
};

}

#include "sensors.inl"
#endif // SENSOR_HPP_