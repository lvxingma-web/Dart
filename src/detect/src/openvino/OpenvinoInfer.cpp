#include "openvino/OpenvinoInfer.hpp"

#include "global/Config.hpp"
#define BLUE 0
#define RED  1
#define NONE_ 2
#define CURR true
#define NEXT false

OpenvinoInfer::OpenvinoInfer(std::map<std::string, std::string> &path) {
    this->model_ = this->core_.read_model(path["XML"], path["BIN"]);
//设置参数
    ov::preprocess::PrePostProcessor ppp = ov::preprocess::PrePostProcessor(this->model_);

    //原始输入配置
    ppp.
            input().//输入基本信息
            tensor().
            set_element_type(ov::element::u8).
            set_layout("NHWC").
            set_color_format(ov::preprocess::ColorFormat::BGR);

    //预处理步骤配置
    ppp.
            input().
            preprocess().
            convert_element_type(ov::element::f32).
            convert_color(ov::preprocess::ColorFormat::RGB).
            scale({255.f, 255.f, 255.f});


    //模型输入配置
    ppp.
            input().
            model().
            set_layout("NCHW");
          
    ppp.
            output(0).
            tensor().
            set_element_type(ov::element::f32);

    ppp.
           output(1).
           tensor().
           set_element_type(ov::element::f32);

    ppp.
            output(2).
            tensor().
            set_element_type(ov::element::f32);


    this->model_ = ppp.build();//根据配置构建模型
    //一个处理当前帧，一个处理下一帧
    this->compiled_model_ = this->core_.compile_model(this->model_, path["DEVICE"]);//编译后模型
    this->compiled_model_next_ = this->core_.compile_model(this->model_, path["DEVICE"]);

    this->infer_requests_.insert(std::pair<bool, ov::InferRequest>(CURR, this->compiled_model_.create_infer_request()));//当前帧推理请求
    this->infer_requests_.insert(std::pair<bool, ov::InferRequest>(NEXT, this->compiled_model_next_.create_infer_request()));//下一帧推理请求
}

std::vector<OpenvinoInfer::Light> OpenvinoInfer::infer(cv::Mat &src, const cv::Size2d &dst_size, const int &my_color, const bool &startup) {
    OpenvinoInfer::Resize resize = OpenvinoInfer::letterBox(src, dst_size);
    // 高效的颜色过滤方法
    cv::Mat hsv, combined_mask;
    
    // 转换到HSV颜色空间
    cv::cvtColor(resize.resized_image, hsv, cv::COLOR_BGR2HSV);
    
    // 一次性创建综合掩膜
    combined_mask = cv::Mat::zeros(hsv.size(), CV_8UC1);
    
    // 遍历图像，只处理需要的像素
    for (int i = 0; i < hsv.rows; i++) {
        for (int j = 0; j < hsv.cols; j++) {
            cv::Vec3b hsv_pixel = hsv.at<cv::Vec3b>(i, j);
            int h = hsv_pixel[0];
            int s = hsv_pixel[1];
            int v = hsv_pixel[2];
            
            // 白灯检测（高亮度，低饱和度）
            bool is_white = (v > 240 && s < 20);
            
            // 黄色检测（特定色调范围）
            bool is_yellow = (h >= 20 && h <= 45 && s > 80);
            
            // 发黄白色检测（低饱和度，高亮度，略带黄色调）
            bool is_yellowish_white = (h >= 10 && h <= 50 && s < 80 && v > 200);
            
            // 如果是需要过滤的颜色，标记为1
            if (is_white || is_yellow || is_yellowish_white) {
                combined_mask.at<uchar>(i, j) = 255;
            }
        }
    }
    
    // 简单的膨胀操作
    cv::dilate(combined_mask, combined_mask, cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3)));
    
    // 将过滤区域设置为灰色
    cv::Mat masked_image = resize.resized_image.clone();
    masked_image.setTo(cv::Scalar(32, 32, 32), combined_mask);
    
    // 使用遮蔽后的图像进行推理
    auto *input_data = (uint8_t *)masked_image.data;
    cv::imshow("masked_image", masked_image);
//    cv::waitKey(1);
    //auto *input_data = (uint8_t *)resize.resized_image.data;

    ov::Tensor input_tensor = ov::Tensor(
            this->compiled_model_.input().get_element_type(),//获取编译后的输入张量元素类型
            this->compiled_model_.input().get_shape(),//获取编译后的输入张量形状
            input_data);

    if (startup) {
        this->infer_requests_[CURR].set_input_tensor(input_tensor);
        this->infer_requests_[CURR].start_async();//异步启动推理请求
        return {};
    }

    this->infer_requests_[NEXT].set_input_tensor(input_tensor); //设置输入张量
    this->infer_requests_[NEXT].start_async();//异步启动推理请求
    this->infer_requests_[CURR].wait();//等待推理完成

//    std::vector<int> _class_ids;
    std::vector<float> confidences;                 //置信度
//    std::vector<double> _sizes;
    std::vector<cv::Rect> boxes;                    //检测每个框的位置和大小信息
    std::vector<OpenvinoInfer::Light> Lights;       //检测结果
    std::vector<int> nms_result;                    //存储非极大值抑制后保留的检测框的索引
    
    const ov::Tensor &outputTensor = this->infer_requests_[CURR].get_output_tensor(0);
    ov::Shape outputShape = outputTensor.get_shape();

    auto *outputData = outputTensor.data<float>();
    
    //遍历检测结果并筛选符合条件的检测框
    for(size_t i = 0 ; i < outputShape[1] ; ++i){//检测框数量
        float* detection = &outputData[i * outputShape[2]];//每个检测框的起始位置
        //normalize the coordinate
        float confidence = detection[4];
        if(confidence >= (double)J_DETECT.config_["openvino"]["score_threshold"]){
            float* classesConfidence = &detection[4 + 1];//提取当前检测框的类别置信度
            float* colorsConfidence = &detection[4 + 1 + 9];//提取当前检测框的颜色置信度
            cv::Mat scores(1, 9, CV_32FC1, classesConfidence);
            cv::Mat colors(1, 4, CV_32FC1, colorsConfidence);
            cv::Point classIdPoint;
            double maxClassConfidence;
            cv::minMaxLoc(scores, nullptr, &maxClassConfidence, nullptr, &classIdPoint);

            if(maxClassConfidence >= (double)J_DETECT.config_["openvino"]["score_threshold"]){

                if (classIdPoint.x != 8) {
                    continue;
                }
                
                OpenvinoInfer::Light Light;

                Light.id = classIdPoint.x;
                Light.score = confidence;
                Light.center_point = cv::Point2f(detection[0], detection[1]);
                Light.box = cv::Rect2d(detection[0] - detection[2] / 2., detection[1] - detection[3] / 2., detection[2], detection[3]);//计算检测框的位置
                Lights.emplace_back(Light);

                confidences.emplace_back(confidence);
                boxes.emplace_back(cv::Rect(detection[0] - detection[2] / 2., detection[1] - detection[3] / 2., detection[2], detection[3]));//计算检测框的大小信息
               
            }
        }
    }

    cv::dnn::NMSBoxes(//非极大值抑制
            boxes,
            confidences,
            J_DETECT.config_["openvino"]["score_threshold"],
            J_DETECT.config_["openvino"]["nms_threshold"],
            nms_result);//储存非极大值抑制后的检测框索引
    std::vector<OpenvinoInfer::Light> result;
    for(const int &idx : nms_result){
        result.emplace_back(OpenvinoInfer::Light {
                Lights[idx].id,           //类别id
                Lights[idx].score,        //置信度
                Lights[idx].box,          //检测框位置和大小信息
                Lights[idx].center_point  //检测框中心点坐标
        });
    }

    //交换当前帧和下一帧的推理请求，避免数据丢失
    auto change = this->infer_requests_[CURR];
    this->infer_requests_[CURR] = this->infer_requests_[NEXT];
    this->infer_requests_[NEXT] = change;
    return result;
}

OpenvinoInfer::Resize OpenvinoInfer::letterBox(cv::Mat &src, const cv::Size2d &dst_size) {
    int w = src.cols;
    int h = src.rows;

    double r_w = (double)dst_size.width / w;
    double r_h = (double)dst_size.height / h;
    double r = std::min(r_w, r_h);

    int new_w = (int)(w * r);
    int new_h = (int)(h * r);

    cv::Mat resized_image;

    cv::resize(src, resized_image, cv::Size(new_w, new_h), cv::INTER_CUBIC);

    cv::Mat canvas(dst_size, CV_8UC3, cv::Scalar(128, 128, 128));

    int dx = (int)((dst_size.width - new_w) / 2.);
    int dy  =(int)((dst_size.height - new_h) / 2.);

    resized_image.copyTo(canvas(cv::Rect(dx, dy, new_w, new_h)));
//     cv::imshow("resized_image", canvas);
//     cv::waitKey(1);
    return {canvas, dx, dy};
}

//调整检测框位置和尺寸
void OpenvinoInfer::fitRec(std::vector<OpenvinoInfer::Light> &bboxes, cv::Size2d ori_size, cv::Size2d now_size) {
    double scale = std::max((double)ori_size.width / now_size.width, (double)ori_size.height / now_size.height);

    for (auto &bbox : bboxes) {
        bbox.box.x = (bbox.box.x - now_size.width / 2) * scale + ori_size.width / 2;    //调整检测框的x坐标
        bbox.box.y = (bbox.box.y - now_size.height / 2) * scale + ori_size.height / 2;  //调整检测框的y坐标
        bbox.box.width *= scale;  //原始图像中的宽度
        bbox.box.height *= scale; //原始图像中的高度
        bbox.center_point.x = (bbox.center_point.x - now_size.width / 2) * scale + ori_size.width / 2;     //调整检测框中心点的x坐标
        bbox.center_point.y = (bbox.center_point.y - now_size.height / 2) * scale + ori_size.height / 2;   //调整检测框中心点的y坐标
    }
}