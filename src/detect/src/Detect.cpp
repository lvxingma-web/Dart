#include "Detect.hpp"

#include "function/function.hpp"
#include "global/Config.hpp"
#include "traditional/traditional_detect.hpp"                                      

//模型推理
Detect::Detect(const char *mode) : mode_(mode) {
    //传统视觉检测
    if(std::string(mode_) == "traditional"){
        this->traditional_detector_ = new GreenLightDetector();
        this ->openvino_infer_ = nullptr;
        //std::cout << "Detector initialized with traditional mode" << std::endl;
    }else{
        //深度学习检测
        std::map<std::string, std::string> path;

        std::string project_root = "/home/ganjie/Vision_Code/";
        std::string xml_path = project_root + "config/openvino/Katrin.xml";
        std::string bin_path = project_root + "config/openvino/Katrin.bin";
        std::string device = "CPU";

        path.insert(std::pair<std::string, std::string>("XML", xml_path));
        path.insert(std::pair<std::string, std::string>("BIN", bin_path));
        path.insert(std::pair<std::string, std::string>("DEVICE", device));

        this->openvino_infer_ = new OpenvinoInfer(path);
        this->traditional_detector_ = nullptr;
        //std::cout << "Detector initialized with deep learning mode" << std::endl;
    }
}

Detect::~Detect() {
    if(this->openvino_infer_){
        delete this->openvino_infer_;
        this->openvino_infer_ = nullptr;
    }
    if(this->traditional_detector_){
        delete this->traditional_detector_;
        this->traditional_detector_ = nullptr;
    }
    std::cout<< "detector destroy"<<std::endl;
}

bool Detect::useTraditionalDetector() const{
    return mode_ == "traditional";
}

void Detect::detect(
        LfStack <FromData> &message_stack,       //待处理的消息栈
        LfStack <ImageInfo> &image_info_stack,   //检测后的图像信息
        LfStack <ImageInfo> &mouse_info_stack) { //鼠标信息栈
    //std::cout<< "detector start"<<std::endl;;
    std::optional <FromData> opt_message_ = message_stack.pop();
    //std::cout<< "detector start"<<std::endl;;
    if (!opt_message_.has_value()) {
        //std::cout<< "detector start"<<std::endl;
        return;
    }

    //std::cout<< "detector start"<<std::endl；

    auto message = std::move(opt_message_.value());

    std::vector<OpenvinoInfer::Light> infer_results;
    //根据检测模式选择不同的检测方法
    if (useTraditionalDetector()){
        //使用传统视觉检测
        if(this->traditional_detector_== nullptr){
            std::cout<< "traditional detector is not null"<<std::endl;
            return;
        }
        auto traditional_results = this->traditional_detector_->detect(message.img);
        
        // 将传统检测结果转换为OpenvinoInfer::Light格式
        for (const auto& result : traditional_results) {
            OpenvinoInfer::Light light;
            light.center_point = result.center;
            light.score = result.confidence;
            // 根据半径创建边界框
            int radius = static_cast<int>(result.radius);
            light.box = cv::Rect(
                static_cast<int>(result.center.x) - radius,
                static_cast<int>(result.center.y) - radius,
                radius * 2,
                radius * 2
            );
            infer_results.push_back(light);
        }
        
        //std::cout << "Traditional detection found " << infer_results.size() << " lights" << std::endl;
    }
    else{
        //使用深度学习检测
        if(this->openvino_infer_== nullptr){
            std::cout<< "openvino infer is not null"<<std::endl;
            return;
        }       
        infer_results = this->openvino_infer_->infer(message.img, cv::Size(640, 384), 0, this->startup_);
    }

    if (this->startup_) {
        this->startup_ = false;
        this->last_message_ = std::move(message);
        return;
    }

    //TODO::delete -> armor.id = 3 && add -> armor.id != 0;
    ImageInfo i_i;
    i_i.cd = this->last_message_.cd;
    i_i.ori_mat = this->last_message_.img;
    i_i.timestamp = this->last_message_.timestamp;

    for (const auto &result : infer_results) {
        LightInfo light_info;
        light_info.center_point = result.center_point;
        light_info.score = result.score;
        light_info.box = result.box;

        i_i.lights.emplace_back(light_info);
    }



    image_info_stack.pop();
    image_info_stack.push(i_i);

    std::cout<< "image info set success"<<std::endl;

    mouse_info_stack.pop();
    mouse_info_stack.push(i_i);

    std::cout<< "mouse info set success"<<std::endl;

    this->last_message_ = std::move(message);
}
