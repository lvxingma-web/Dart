# Dart

- 功能说明： 适用于飞镖本赛季前哨站引导灯识别

## 项目结构

```text
├── .gitignore
├── CMakeLists.txt
├── config
│   ├── flatbuffers
│   ├── json
│   └── openvino
├── LICENSE
├── main.cpp
├── README.md
└── src
    ├── capture
    │   ├── CMakeLists.txt
    │   ├── inc
    │   │   └── Capture.hpp
    │   └── src
    │       └── Capture.cpp
    ├── detect
    │   ├── CMakeLists.txt
    │   ├── inc
    │   │   ├── Detect.hpp
    │   │   └── openvino
    │   │       └── OpenvinoInfer.hpp
    │   └── src
    │       ├── Detect.cpp
    │       └── openvino
    │           └── OpenvinoInfer.cpp
    ├── mouse
    │   ├── CMakeLists.txt
    │   ├── inc
    │   │   └── Mouse.hpp
    │   └── src
    │       └── Mouse.cpp
    ├── sensors
    │   ├── CMakeLists.txt
    │   ├── inc
    │   │   ├── hikrobot
    │   │   │   ├── HikRobot.hpp
    │   │   │   └── inc
    │   │   │       ├── CameraParams.h
    │   │   │       ├── MvCameraControl.h
    │   │   │       ├── MvErrorDefine.h
    │   │   │       ├── MvISPErrorDefine.h
    │   │   │       └── PixelType.h
    │   │   ├── sensors.hpp
    │   │   ├── sensors.inl
    │   │   └── serialport
    │   │       ├── CRC_Check.h
    │   │       └── SerialPort.h
    │   └── src
    │       ├── hikrobot
    │       │   └── HikRobot.cpp
    │       ├── sensors.cpp
    │       └── serialport
    │           ├── CRC_Check.cpp
    │           └── SerialPort.cpp
    ├── threads
    │   ├── CMakeLists.txt
    │   └── inc
    │       ├── lockfree
    │       │   ├── LockFree.hpp
    │       │   └── LockFree.inl
    │       └── threadpool
    │           └── ThreadPool.hpp
    ├── track
    │   ├── CMakeLists.txt
    │   ├── inc
    │   │   ├── filter
    │   │   │   └── OneEuroFilter.hpp
    │   │   └── Track.hpp
    │   └── src
    │       ├── filter
    │       │   └── OneEuroFilter.cpp
    │       └── Track.cpp
    └── utility
        ├── CMakeLists.txt
        ├── inc
        │   ├── dump
        │   │   └── dump.hpp
        │   ├── function
        │   │   └── function.hpp
        │   ├── global
        │   │   ├── Config.hpp
        │   │   └── Struct.hpp
        │   ├── json
        │   │   └── ReJson.hpp
        │   └── mcap
        └── src
            ├── function
            │   └── function.cpp
            └── json
                └── ReJson.cpp
```