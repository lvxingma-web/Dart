#include "traditional/traditional_detect.hpp"
#include "global/Config.hpp"
#include <algorithm>
#include <opencv2/imgproc.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/video/tracking.hpp>   // 时序一致性

GreenLightDetector::GreenLightDetector()
{
    /* -------- 颜色 -------- */
    hsvLow_  = cv::Scalar(45, 80, 40);
    hsvHigh_ = cv::Scalar(85, 255, 255);
    diffTh_  = 20;

    /* -------- 近距轮廓 -------- */
    minArea_   = 30;
    minRadius_ = 4;
    maxRadius_ = 100;
    minConf_   = 0.7f;

    /* -------- 远距 blob（低曝光抗噪） -------- */
    minAreaFar_    = 1;
    minRadiusFar_  = 1.0f;
    minConfFar_    = 0.20f;      // 对比度门槛
    blobMinThresh_ = 3;          // 去掉1-2px碎斑

    kernelSmall_ = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
    clahe_       = cv::createCLAHE(3.0, cv::Size(8, 8));
}

void classifyLights(std::vector<Light>& lights, int imgRows)
{
    const float cutRow = imgRows * 0.52f;
    for (auto& l : lights)
        l.type = (l.center.y > cutRow) ? LightType::OUTPOST : LightType::BASE;
}

std::vector<Light> GreenLightDetector::detect(const cv::Mat& bgr)
{
    cv::Mat gray, hsv, maskDiff, maskHsv, maskFinal;
    std::vector<cv::Mat> channels(3), hsvChannels;

    /* 0. 灰度图（时序+对比度用） */
    cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);

    /* 1. RGB差分 */
    cv::split(bgr, channels);
    cv::Mat diff1, diff2;
    cv::subtract(channels[1], channels[2], diff1);
    cv::subtract(channels[1], channels[0], diff2);
    cv::threshold(diff1, diff1, diffTh_, 255, cv::THRESH_BINARY);
    cv::threshold(diff2, diff2, diffTh_, 255, cv::THRESH_BINARY);
    cv::bitwise_and(diff1, diff2, maskDiff);

    /* 2. HSV + CLAHE */
    cv::cvtColor(bgr, hsv, cv::COLOR_BGR2HSV);
    cv::split(hsv, hsvChannels);
    clahe_->apply(hsvChannels[2], hsvChannels[2]);
    cv::merge(hsvChannels, hsv);
    cv::inRange(hsv, hsvLow_, hsvHigh_, maskHsv);
    cv::bitwise_and(maskHsv, maskDiff, maskFinal);

    /* 3. 形态学开运算 */
    cv::morphologyEx(maskFinal, maskFinal, cv::MORPH_OPEN, kernelSmall_);

    std::vector<Light> lights;

    /* 4. 近距轮廓分支（4-12 m） */
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(maskFinal, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    for (const auto& cnt : contours)
    {
        double area = cv::contourArea(cnt);
        if (area < minArea_) continue;
        cv::Point2f center; float radius;
        cv::minEnclosingCircle(cnt, center, radius);
        if (radius < minRadius_ || radius > maxRadius_) continue;
        double circleArea = CV_PI * radius * radius;
        float conf = static_cast<float>(area / circleArea);
        if (conf < minConf_) continue;
        lights.emplace_back(Light{center, radius, conf});
    }

    /* 5. 远距blob分支（14-26 m） */
    cv::SimpleBlobDetector::Params p;
    p.minArea = 1;  p.maxArea = 80;
    p.minThreshold = blobMinThresh_;
    p.thresholdStep = 5;
    p.filterByCircularity = p.filterByConvexity = p.filterByInertia = false;
    cv::Ptr<cv::SimpleBlobDetector> blobDetector = cv::SimpleBlobDetector::create(p);

    std::vector<cv::KeyPoint> kps;
    blobDetector->detect(maskFinal, kps);

    for (const auto& kp : kps)
    {
        float r = kp.size / 2.0f;
        if (r < 1.0f || r > 7.0f) continue;
        /* core-ring 对比度 */
        cv::Mat patch;
        cv::getRectSubPix(gray, cv::Size(7, 7), kp.pt, patch);
        float core = static_cast<float>(patch.at<uchar>(3, 3));
        cv::Mat ringMask = cv::Mat::ones(7, 7, CV_8U) * 255;
        cv::circle(ringMask, cv::Point(3, 3), 2, cv::Scalar(0), -1);
        float ring = static_cast<float>(cv::mean(patch, ringMask)[0]);
        float conf = (core - ring) / (ring + 1.0f);
        if (conf < minConfFar_) continue;
        lights.emplace_back(Light{kp.pt, r, conf});
    }

    /* 6. 头顶白灯颜色+空间过滤 */
    for (auto it = lights.begin(); it != lights.end();)
    {
        cv::Vec3b px = bgr.at<cv::Vec3b>(static_cast<int>(it->center.y),
                                          static_cast<int>(it->center.x));
        float gRatio = static_cast<float>(px[1]) / (px[0] + px[1] + px[2] + 1);
        if (gRatio < 0.38f || it->center.y < bgr.rows * 0.05f)
            it = lights.erase(it);
        else
            ++it;
    }

/* 7. 时序2帧一致性 */
if (!prevGray_.empty() && !prevPts_.empty())   // ← 加后半句
{
    std::vector<cv::Point2f> currPts;
    for (const auto& l : lights) currPts.push_back(l.center);

    std::vector<uchar> status;
    cv::calcOpticalFlowPyrLK(prevGray_, gray, prevPts_, currPts,
                             status, cv::noArray(), cv::Size(3, 3), 1,
                             cv::TermCriteria(cv::TermCriteria::COUNT + cv::TermCriteria::EPS, 3, 0.1));
    std::vector<Light> stable;
    for (size_t i = 0; i < lights.size(); ++i)
        if (status[i]) stable.push_back(lights[i]);
    lights.swap(stable);
}

    /* 8. NMS留最强 */
    if (lights.size() > 1)
    {
        std::sort(lights.begin(), lights.end(),
                  [](const Light& a, const Light& b)
                  { return a.confidence > b.confidence; });
        const float keepDist = 20;
        std::vector<Light> final;
        final.push_back(lights[0]);
        for (size_t i = 1; i < lights.size(); ++i)
        {
            bool overlap = false;
            for (const auto& k : final)
                if (cv::norm(lights[i].center - k.center) < keepDist)
                { overlap = true; break; }
            if (!overlap) final.push_back(lights[i]);
        }
        lights.swap(final);
    }

    /* 9. 基地/前哨站分类 */
    classifyLights(lights, bgr.rows);
    return lights;
}


/* ---------- 画结果 ---------- */
void GreenLightDetector::drawResult(cv::Mat& bgr,
                                    const std::vector<Light>& lights)
{
    for (const auto& l : lights)
    {
        // 1. 四点方框
        cv::RotatedRect rRect(l.center,
                              cv::Size2f(l.radius * 2, l.radius * 2),
                              0.0f);
        cv::Point2f vertices[4];
        rRect.points(vertices);
        for (int i = 0; i < 4; ++i)
            cv::line(bgr, vertices[i], vertices[(i + 1) % 4],
                     cv::Scalar(0, 0, 255), 1.5
                    );

        // 2. 对角线两条
        cv::line(bgr, vertices[0], vertices[2], cv::Scalar(0, 0, 255), 1);
        cv::line(bgr, vertices[1], vertices[3], cv::Scalar(0, 0, 255), 1);

        // 3. 文字标注
        cv::putText(bgr,
                    (l.type == LightType::BASE ? "BASE" : "OUTPOST"),
                    l.center + cv::Point2f(5, l.radius + 10),
                    cv::FONT_HERSHEY_SIMPLEX, 0.4,
                    cv::Scalar(255, 0, 255), 1);
    }
}
