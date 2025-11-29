#ifndef __SERIAL_PORT_H__
#define __SERIAL_PORT_H__

#include "global/Struct.hpp"

class SerialPort {
public:
    explicit SerialPort();
    ~SerialPort();

    bool init();
    bool receive(CarData &cd);
    void send(VisionData &vd);
    void closePort();

private:
    void setBrate();
    int setBit();//设置数据位、停止位、校验位
    int fd_;//文件描述符
    int baudrate_;
    int databits_;//数据位
    int stopbits_;//停止位
    int parity_;//校验位
    unsigned char r_data_[4096];//接收数据
    unsigned char t_data_[30];//发送数据
};
#endif