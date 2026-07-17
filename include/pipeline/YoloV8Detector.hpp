#pragma once

#include "pipeline/DetectionClasses.hpp"
#include "pipeline/IDetector.hpp"

#include <opencv2/dnn.hpp>

#include <memory>
#include <string>

namespace pipeline {

class YoloV8Detector : public IDetector {
public:
    static constexpr int kInputSize = 640;

    explicit YoloV8Detector(const std::string& model_path,
                            float conf_threshold = kDefaultConfThreshold,
                            float nms_iou_threshold = kDefaultNmsIouThreshold);

    std::vector<Detection> process(const Frame& frame) override;

    float conf_threshold() const { return conf_threshold_; }
    float nms_iou_threshold() const { return nms_iou_threshold_; }

private:
    struct LetterboxParams {
        float scale = 1.0f;
        float pad_x = 0.0f;
        float pad_y = 0.0f;
    };

    LetterboxParams compute_letterbox(int src_w, int src_h) const;
    std::vector<Detection> decode_output(const cv::Mat& output, const LetterboxParams& letterbox,
                                         int frame_w, int frame_h) const;

    cv::dnn::Net net_;
    float conf_threshold_;
    float nms_iou_threshold_;
};

}  // namespace pipeline
