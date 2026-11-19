# Dart 视觉代码交接

本文供换设备后接手仓库的人或 Codex 阅读。内容以 2026-11-20 的 `main` 提交 `e7d9cee` 为基线。换设备、换分支或拉取新提交后，先用 `git status --short --branch` 和 `git rev-parse --short HEAD` 确认版本，再核对下文引用的源码与配置值。

## 1. 应用场景和阅读入口

Dart 是机械飞镖比赛设备的视觉端程序，用于观察绿色引导灯、检测与跟踪灯的位置，并把像素误差和跟踪状态经串口发给电控。当前仓库提供视觉部分，机构如何使用误差和电控各状态的完整定义需要查电控工程。

记录中的部署设备为 Ubuntu 22.04、Intel N100 小电脑、海康 MV-CS020-10UC USB 相机、MVL-HF1229M-6MPR 镜头和本地图形桌面。现场联调走海康实时采集和串口。相机单独检查使用 MVS 预览，代码回归可以使用合成图和虚拟串口。录像是可选构建功能，当前未启用。[相机测试记录](MVS_测试记录.md)

| 想了解或修改 | 首先查看 |
| --- | --- |
| 程序入口、工作线程、录像启用条件 | [main.cpp](../main.cpp)、[CMakeLists.txt](../CMakeLists.txt) |
| 取图、曝光、裁剪、图像旋转 | [HikRobot.cpp](../src/sensors/src/hikrobot/HikRobot.cpp)、[sensors.json](../config/json/sensors.json) |
| 串口布局、接收、发送、CRC | [Struct.hpp](../src/utility/inc/global/Struct.hpp)、[SerialPort.cpp](../src/sensors/src/serialport/SerialPort.cpp)、[CRC_Check.cpp](../src/sensors/src/serialport/CRC_Check.cpp) |
| 绿灯检测、检测结果入栈 | [Detect.cpp](../src/detect/src/Detect.cpp)、[traditional_detect.cpp](../src/detect/src/traditional/traditional_detect.cpp) |
| 跟踪状态、`begin` 和像素误差 | [Track.cpp](../src/track/src/Track.cpp)、[track.json](../config/json/track.json)、[ktrack.json](../config/json/ktrack.json) |
| 调试画面、缩放与标记 | [Mouse.cpp](../src/mouse/src/Mouse.cpp)、[mouse.json](../config/json/mouse.json)、[keep.json](../config/json/keep.json) |
| 换机编译、启动、既有联调记录 | [Dart_upstart.sh](../Dart_upstart.sh)、[Ubuntu22 / N100 联调说明](Ubuntu22_N100_联调说明.md) |

## 2. 从进程启动到返回结果

```text
main 创建 SerialPort，并启动采集、检测、跟踪、鼠标调试四项任务
  采集  每轮先收有效 CarData → 取相机帧 → 转 BGR、旋转 → FromData
  检测  FromData → GreenLightDetector → ImageInfo（候选灯位置）
         ├─ 跟踪 → 选灯、更新状态和误差 → VisionData → 串口回传
         └─ 鼠标 → 预览、缩放、标记
  录像  编译启用时，从跟踪任务取得 CaptureInfo 并写入 MCAP
```

- `main()` 从 `camera.type` 选择相机。当前为 `hikrobot`，并直接创建 `Detect("traditional")` 与 `Track(sp, "debug")`。`detect.json` 里的 `detection.mode=deep_learning` 无法切换这个入口。
- `BaseCamera::refreshImage()` 调用 `getImage()`，再发布 `FromData`，其中含图像、主机时间戳和电控 `CarData`。图像传给检测后，`ImageInfo` 分别发布到跟踪栈和鼠标栈，包含候选灯、图像、时间戳与电控字段。启用录像时，跟踪还将图像副本与两方向业务数据封为 `CaptureInfo`。
- `ThreadPool::addTask()` 为持续循环的任务启动工作线程，`run()` 等待工作线程结束。各通道使用 `LfStack`，生产方先 `pop()` 清理旧消息再 `push()` 新消息，以处理新帧为主。调试时不要假定每帧都会经过每个模块。
- **当前传统分支存在帧配对问题。** `Detect::detect()` 处理当前 `message.img`，却从 `last_message_` 取图像、电控字段和时间戳来组装结果。第一条输入只建立缓存。此后叠加框和用于 `begin` 判断的 `mode/number` 可能与产生检测点的帧不一致。OpenVINO 双请求分支设计为返回上一输入的结果，改配对时需分别核对两条路径。

[入口](../main.cpp) · [采集发布](../src/sensors/inc/sensors.inl) · [数据结构](../src/utility/inc/global/Struct.hpp) · [检测发布](../src/detect/src/Detect.cpp) · [工作线程](../src/threads/inc/threadpool/ThreadPool.hpp) · [消息栈](../src/threads/inc/lockfree/LockFree.inl)

## 3. 取图、曝光与配置生效范围

`HikRobot::cameraInit()` 枚举 USB 设备、打开相机、设置参数，然后由 `grapImage()` 开始采集。`getImage()` 每次取图前循环调用 `SerialPort::receive()`，失败时等待约 5 ms 再试，因此 **MVS 能预览，Dart 仍可能因没有有效电控包而不出图**。收到包后先调用 `cameraModeChange()`，再用 `MV_CC_GetImageBuffer()` 取帧。单次 SDK 请求超时参数为 1000 ms。持续取帧失败超过 `camera.drop_time`（当前 10 秒）时进程退出。这段计时从开始请求相机帧算起。

| 设置 | 当前海康代码行为 |
| --- | --- |
| 触发与像素格式 | `TriggerMode=OFF` 连续采集，`BayerRG8` 经 SDK 转 `BGR8`。 |
| 采集区域 | `setResolution(-1, 384, 680, -1, 640, 0)` 请求宽 384、高 640、`OffsetX=680`、`OffsetY=0`。转置加两次翻转等效于逆时针旋转 90°，若相机接受该 ROI，输出为宽 640、高 384。 |
| 目标帧率 | 源码请求 `250.0 fps`。设备接受值和端到端帧率需实测，后者也受串口、曝光和各线程处理速度影响。 |
| 初始化曝光、增益 | 源码请求 `3000 µs` 和增益 `8.0`。 |
| `hikrobot.exposure.red_exposure` | 当前值 `10000 µs`。首次进入或切换至 `mode=0/1` 时，`my_color=0` 选择此值，设置整幅图像的曝光。`mode=2` 沿用此前值。 |
| `hikrobot.exposure.blue_exposure` | 当前值 `4500 µs`，仅 `my_color=1` 时使用。取图代码固定 `my_color=0`。红蓝是曝光档的名字，传统算法始终找绿灯。 |
| `exposure.normal` / `auto`，`gain` | JSON 中 `normal=10000`、`auto=0`、`gain.value=17`，当前海康初始化不读取这些字段。 |
| `white_balance` | 白平衡用于按颜色通道校正偏色。配置为 `auto=0`、红 `1.6`、绿 `1.0`、蓝 `1.0`，当前后端没有把这段配置下发给相机。 |
| `width`、`height`、`channels`、`offset.v_offset` | 当前 JSON 为 `828`、`650`、`3` 和 `304`。实际海康裁剪和像素格式由上面的初始化代码设置。 |
| `cam`、`cam1`、`dis` | 留存的相机矩阵与畸变配置。实时识别、跟踪、误差计算没有读取。当前海康 `cam1` 声明 3×3，数据却有 11 项，接入时要先核实矩阵含义和格式。 |

修改 `red_exposure` 后重启 Dart。修改初始化曝光、增益、采集区域或帧率的 `.cpp` 源码后需要重新编译。日志 `val = ...` 出现在 SDK 调用前，表示请求值。判断亮度时应结合设置错误和相机读回值。历史 MVS 预览使用过曝光 `30000 µs` 和增益 `8`，这组值对应当时的镜头和采集条件。历史联调说明记录的 `4500 µs` 与 `1624×1240` 标定表也不同于本次所读的配置，以当前文件为准。

[海康后端](../src/sensors/src/hikrobot/HikRobot.cpp) · [传感器配置](../config/json/sensors.json) · [MVS 预览记录](MVS_测试记录.md)

## 4. 绿灯检测怎么运行

实际入口为 `Detect::detect()` → `GreenLightDetector::detect()`。传统检测参数主要写在 C++ 中，修改 `detect.json` 的模型置信度不会改变传统检测。

| 步骤 | 实现与主要阈值 |
| --- | --- |
| 颜色候选 | `G-R > 20` 与 `G-B > 20`。将 BGR 转 HSV，对 V 通道做 CLAHE（clip 3、8×8），与 `H[45,85] S[80,255] V[40,255]` 掩膜取交集。 |
| 去噪 | 对交集掩膜做 3×3 椭圆核开运算。 |
| 轮廓候选 | 外轮廓面积至少 30，最小外接圆半径 4～100 像素，面积与外接圆面积的比值至少 0.7。 |
| Blob 候选 | 与轮廓分支在同一帧执行。`SimpleBlobDetector` 的面积参数为 1～80、起始阈值 3，再按半径 1～7 像素和中心对周边灰度对比度 ≥0.20 筛选。 |
| 过滤与合并 | 去掉中心绿通道比例低于 0.38 或落在上方 5% 的候选。按分数排序，近于 20 像素的候选保留分数更高者。 |
| 返回 | `classifyLights()` 根据中心 y 位于图高 52% 的哪侧设类别。`Detect` 对接到 `LightInfo` 时保留中心、框和分数，类别没有传给 `Track`。 |

源码包含两帧 LK 光流筛选，但没有给 `prevGray_`、`prevPts_` 写入历史，所以当前这段没有运行。传统检测中“近距”和“远距”是两条候选生成分支，现场可检距离需要另测。

备用 `OpenvinoInfer` 分支使用硬编码的 Katrin `.xml/.bin`、CPU 和 `640×384` 输入尺寸，异步请求并读取 `detect.json` 的 `openvino.score_threshold`、`nms_threshold`（各 0.3）。当前启动未加载模型。构建仍依赖 OpenVINO，因为传统和模型代码一起编译链接。

[传统检测](../src/detect/src/traditional/traditional_detect.cpp) · [检测分发](../src/detect/src/Detect.cpp) · [备用 OpenVINO 实现](../src/detect/src/openvino/OpenvinoInfer.cpp)

## 5. 跟踪、`begin` 和像素误差

`Track::track()` 消费 `ImageInfo`，在 `debug` 模式下检查 `track.json` 的文件修改时间，再由 `updateTimeInterval()` 和 `chooseTarget()` 更新状态。`updateState()` 累计的是**图像时间间隔的秒数**，虽然参数名带 `_frame`。

| 状态或过程 | 当前行为 |
| --- | --- |
| `LOST` | 有候选的累计时间达到 `min_found_frame=1.0` 才进入 `FOUND`，初次选分数最高的灯。未建立目标时发送 `yaw_error=0`、`target_status=0`。 |
| `FOUND` | 有候选时选择距上次目标中心最近的灯，目前没有最大匹配距离限制。累计发现时间使用 `limit_found_frame=1.5`。 |
| `DROP` | 短时无候选，沿用旧中心，仍走误差输出逻辑并发送 `target_status=1`。`max_drop_frame=1.5` 达到后重置为 `LOST`，`limit_drop_frame` 也是 1.5。 |
| 绘图滤波 | x、y 各有一个 One Euro 滤波器，初始化读取 `min_cut_off=1.5`、`beta=0.6`、`d_cut_off=0.4`。滤波点用于窗口绘制，误差使用检测中心原值。 |

`Track::updateVisionData()` 在状态为 `FOUND` 或短时 `DROP` 时执行。当 `number=0` 且 `mode=0`，它将目标中心的 **y 坐标**写入 `ktrack.json` 的 `begin`，随后重新打开文件读取。这是图像坐标中的基准记录，和相机内参标定分别独立。它持续覆盖 `begin`，直到程序处理到不再满足条件的输入。短时 `DROP` 仍可写入上一次的目标位置。运行中只读观察可另开终端执行 `watch -n 0.5 cat config/json/ktrack.json`，命令从项目根目录运行。

```text
yaw_error = begin - 目标中心 y - group[choice]["0"] - group[choice][str(number)]
```

- 误差单位为旋转后的图像像素。图像坐标 y 向下增大。设备的物理方向取决于相机安装和电控解释。`begin` 是程序记录的参考点，当前约为 `225.98 px`。它不会自动取画面中心或相机内参 `cy`。
- `track.json` 的 `choice` 当前为 `82_84_88_105`，选择该组下的公共补偿与 `number` 对应补偿，键 `0`～`4` 目前全为 0。`number=0` 时两项都索引 `"0"`，历史公式会扣两次。未配置编号不用来表达新业务语义。
- `track.json` 可在跟踪任务中按修改时间重读，已经初始化的滤波器不会因此马上刷新参数。`ktrack.path` 在 `Track` 构造时读取。手工修改 `ktrack.json` 通常重启才会生效，保存基准时的代码会主动重开它。相机配置没有热重载。

[跟踪实现](../src/track/src/Track.cpp) · [跟踪参数](../config/json/track.json) · [基准文件](../config/json/ktrack.json)

## 6. 串口协议和电控字段

当前设备路径为 `/dev/ttyACM0`，串口为 `921600 8N1`。`SerialPort::init()` 从 JSON 读取设备路径、波特率、数据位和停止位，奇偶校验由源码固定为 `N`。两方向数据结构均为 packed，偏移从 0 起算。

| 方向 | 长度 | 字节偏移和字段 |
| --- | --- | --- |
| 电控 → 视觉 `CarData` | 8 字节 | `0` 帧头 `0xA5`，`1` mode，`2` CRC8，`3` status，`4` number，`5` dune，`6–7` CRC16。 |
| 视觉 → 电控 `VisionData` | 9 字节 | `0` 帧头 `0xA5`，`1` CRC8，`2–5` `yaw_error`（小端 32 位 `float`），`6` `target_status`，`7–8` CRC16。 |

CRC8 初值 `0xFF`，电控包覆盖前 2 字节，视觉包覆盖前 1 字节。CRC16 初值 `0xFFFF`，使用高字节查表、左移更新，分别覆盖前 6 和前 7 字节，尾部先低字节后高字节。视觉发送先填 CRC8 再填 CRC16，CRC16 会覆盖 CRC8 字节。现有仓库测试提供固定向量用于验证。[协议结构](../src/utility/inc/global/Struct.hpp) · [CRC 实现](../src/sensors/src/serialport/CRC_Check.cpp) · [协议回归](../tests/serial_crc_regression.cpp)

| 电控字段 | 当前视觉端如何使用 |
| --- | --- |
| `mode` | 注释约定 `0` 不开自瞄且不录像，`1` 开自瞄并录像，`2` 只录像。视觉对三种模式都会继续采集、检测、跟踪并回传。进入 `0/1` 时可设置曝光，`0` 和 `number=0` 组合可记录 `begin`，开启录像后 `0` 跳过记录。是否驱动机构由电控决定。 |
| `number` | 选编号补偿，`0` 和 `mode=0` 组合允许覆盖基准。配置目前列出 `0`～`4`。它也参与窗口显示及可选录像。 |
| `status` | 窗口显示及可选录像。视觉没有定义各值含义，也没有用它决定状态转换。需查电控赋值处。 |
| `dune` | 注释约定 `0` 完全展开，`1` 完全关闭，`2` 正在开合。窗口显示它。开启录像时 `dune=1` 跳过记录，曝光、跟踪、基准写入均不依赖它。 |

`SerialPort::receive()` 查询可读字节，单次最多读取 4096 字节。它从首个 `0xA5` 计算 8 字节边界，只检查最后一个完整候选的帧头与两个 CRC。当前不保存下一次读取所需的残帧，也不在最后一帧校验失败时回退到前一候选。`send()` 遇到 `write()` 返回 `-1` 会重开串口，未重发本帧，也没有处理短写。采集与跟踪线程共享同一个 `SerialPort` 对象，改重连逻辑时应一并检查接收与发送的协调。[串口实现](../src/sensors/src/serialport/SerialPort.cpp)

## 7. 窗口和录像

| 输出 | 从哪里来、能看什么 |
| --- | --- |
| `img` | 海康后端显示转色、旋转后的相机图像。需要有效串口输入才能走到取图。 |
| `track` | 在 `updateVisionData()` 中显示目标框、滤波位置、分数、误差和 `number/mode/status/dune`。`LOST` 时发送状态，但不刷新此窗口。 |
| `mouse` | 在 `Mouse::mouseControl()` 中消费图像副本。支持画面框选、缩放、平移和标记。`mouse.json` 指定缩略图比例 0.4、滚轮倍率 0.5、最小比例 0.05，`keep.json` 提供初始框和标记。保存入口存在，正常绘制路径里的调用被注释。界面框选不改变相机 ROI 或检测输入。 |

`DART_CAPTURE=OFF` 是当前构建状态。启用后，录制线程在有数据、`mode≠0` 且 `dune≠1` 时写帧。`Capture::update()` 将 BGR 图像、`CarData` 和 `VisionData` 的业务字段以 `foxglove.CaptureInformation` 写入 MCAP 的 `/Dart` 主题。文件位于 `../bag/`，按本地时间命名，每 `capture.json` 的 `frame_cnt` 帧轮换（当前 2000）。落盘时间使用写入时的主机时钟。`capture.json` 的 `mcap.directory/name/type` 用于旧回放入口，当前 CMake 明确拒绝 `DART_DUMP=ON`。

[鼠标界面](../src/mouse/src/Mouse.cpp) · [录像分支](../main.cpp) · [序列化实现](../src/capture/src/Capture.cpp) · [录像配置](../config/json/capture.json)

## 8. 在另一台设备构建和运行

以下命令假定已在新设备克隆仓库，并进入仓库根目录。准备 CMake ≥3.25、Ninja、支持 C++20 的编译器、带 GUI 的 OpenCV 开发库、`nlohmann/json.hpp`、海康 MVS SDK、OpenVINO C++ Runtime 开发文件、LZ4 和 Zstd 开发库。当前即使关闭录像，构建目标仍会编译并链接录像模块。只有启用 `DART_WITH_DAHENG` 才需准备大恒 Galaxy SDK。

```bash
bash scripts/setup_flatbuffers.sh
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DDART_DUMP=OFF \
  -DDART_CAPTURE=OFF \
  -DDART_WITH_DAHENG=OFF
cmake --build build --parallel 2
bash scripts/test_serial_crc.sh
```

- FlatBuffers 脚本用 `curl`、`tar`、`sha256sum` 等工具下载、校验并解包指定版本，在仓库内 `.deps/flatbuffers-25.2.10` 安装库和头文件。CMake 要求精确的 25.2.10，使用仓库已提交的 `config/autogenerated_flatbuffers/`，无需重生成。`build/` 和 `.deps/` 是机器本地产物，应在目标机重新生成。[依赖脚本](../scripts/setup_flatbuffers.sh) · [构建定义](../CMakeLists.txt)
- 原机配置缓存是 Ninja、Debug、OpenVINO 2025.1.0 C++ 开发配置。示例用 `RelWithDebInfo` 作为换机编译配置。SDK 安装在非默认位置时，在 CMake 中按实际路径指定 `OpenVINO_DIR` 或 `MVS_CAMERA_CONTROL_LIBRARY`。海康库的查找提示目录为 `/opt/MVS/lib/64`，OpenVINO 的构建提示目录和启动搜索目录分别见对应 CMake 与脚本。[相机链接配置](../src/sensors/CMakeLists.txt) · [检测链接配置](../src/detect/CMakeLists.txt)
- `bash scripts/test_serial_crc.sh` 通过 `pkg-config` 查找 `opencv4`，在私有 PTY 中检查固定 CRC 向量、真实收发函数、错误包、短包和批量包，并启用 UBSan 及边界检查。它可以在接入相机和电控前执行。[测试脚本](../scripts/test_serial_crc.sh)

实机调试先用 MVS 独立确认图像，再关闭 MVS 的相机连接。在 `sensors.json` 中核对设备路径与串口访问权限，保证电控持续发送当前协议的有效帧。登录本地图形桌面后，在仓库根目录运行启动脚本。

```bash
bash Dart_upstart.sh
```

脚本根据自身位置进入 `build/`，补充常用 MVS 与 OpenVINO 动态库目录，再 `exec ./Dart`。项目运行时使用 `../config/json/`，因此从仓库根目录直接执行 `./build/Dart` 会让配置路径按错误的工作目录解析。进入 `build/` 后也可以直接启动已配置运行库的可执行文件。手动运行时用 `Ctrl+C` 结束进程。

源码保存后用 `cmake --build build --parallel 2` 增量编译，Ninja 的 `no work to do` 表示没有需要重编译的输入。改 `sensors.json` 后重启即可。登录自启动模板位于 `config/autostart/dart.desktop.in`，需要在目标机替换 `@DART_ROOT@` 后安装为用户自启动项。执行时机是登录桌面后。[启动脚本](../Dart_upstart.sh) · [自启动模板](../config/autostart/dart.desktop.in)

## 9. 排查入口与后续改进

| 现象 | 从哪一段开始查 |
| --- | --- |
| 一直 `don't have data`、`don‘t have data` 或 `ioctl` 报错 | 查实际设备路径、权限、持续发包、帧长度及 CRC。摄像头采集每轮都等待有效包。 |
| MVS 画面正常，Dart 画面偏暗 | 查 `cameraInit()`、当前模式的曝光覆盖、SDK 设置返回值与相机读回值。MVS 中最后一次设置可能在启动时被覆盖。 |
| 有 `img`，`track` 画面不更新 | 先确认灯在实际采集 ROI 内，查看候选掩膜和跟踪累计时间。`LOST` 分支不刷新 `track`。 |
| `begin` 一直变，或切换模式后仍变了一次 | 查画面里的 `mode/number`、跟踪状态和 `Detect::detect()` 的前后帧配对，再核对文件时间和路径。 |
| 换机执行时报找不到 JSON 或共享库 | 先用启动脚本确认工作目录，按目标机 SDK 路径检查 `ldd build/Dart`。有显示错误时检查本地图形会话和 OpenCV GUI。 |

后续可将采集参数集中到相机配置并读回实际应用值。修复帧配对后验证图像、电控字段、检测点和时间戳是否同源。串口接收可补上跨次读取的残帧处理。若双方确认所有机械镖共用一个偏差，可以考虑把 `number` 保留为原包内遥测字段，将视觉侧的基准记录和状态切换独立出来。若要改变 `mode`、`number` 的语义，先与电控同步约定，保留原报文布局可以减少迁移范围。以上是后续设计选项，当前代码仍按第 5、6 节运行。

核对相机资源释放、`LfStack` 并发访问和多个线程调用 OpenCV 窗口也适合单独做测试。改相机分辨率或旋转时，要一起核对 `begin`、鼠标框、画面与标定坐标；接入内参前先确认标定图像尺寸和矩阵形状。

本次交接复跑了私有 PTY 串口回归，6 组均通过，并确认本机 CMake 构建没有待编译项。历史记录还包含 MVS 单独预览、项目编译、合成绿点的传统检测和 OpenVINO 模型的离线检查。实机长时间稳定性、实际帧率与电控闭环状态应以设备上的验收记录为准。[环境与既有验证](Ubuntu22_N100_联调说明.md) · [CRC 修改记录](../modify/CRC校验问题与修改记录.md)
