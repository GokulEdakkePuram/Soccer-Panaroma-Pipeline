#include "pipeline/YoloV8Detector.hpp"

#include "pipeline/YoloPostprocess.hpp"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace pipeline {

namespace {

bool is_target_class(int class_id) {
    return class_id == kPersonClassId || class_id == kSportsBallClassId;
}

}  // namespace

YoloV8Detector::YoloV8Detector(const std::string& model_path, float conf_threshold,
                               float nms_iou_threshold)
    : conf_threshold_(conf_threshold), nms_iou_threshold_(nms_iou_threshold) {
    net_ = cv::dnn::readNetFromONNX(model_path);
    if (net_.empty()) {
        throw std::runtime_error("Failed to load ONNX model: " + model_path);
    }
    net_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
}

YoloV8Detector::LetterboxParams YoloV8Detector::compute_letterbox(int src_w, int src_h) const {
    LetterboxParams params;
    params.scale = std::min(static_cast<float>(kInputSize) / static_cast<float>(src_w),
                            static_cast<float>(kInputSize) / static_cast<float>(src_h));
    const float new_w = static_cast<float>(src_w) * params.scale;
    const float new_h = static_cast<float>(src_h) * params.scale;
    params.pad_x = (static_cast<float>(kInputSize) - new_w) * 0.5f;
    params.pad_y = (static_cast<float>(kInputSize) - new_h) * 0.5f;
    return params;
}

std::vector<Detection> YoloV8Detector::decode_output(const cv::Mat& output,
                                                     const LetterboxParams& letterbox,
                                                     int frame_w, int frame_h) const {
    // YOLOv8 ONNX output: [1, 84, num_predictions]
    CV_Assert(output.dims == 3);
    CV_Assert(output.size[0] == 1);
    CV_Assert(output.size[1] == 84);

    const int num_predictions = output.size[2];
    const int num_classes = output.size[1] - 4;

    std::vector<Detection> detections;
    detections.reserve(64);

    for (int i = 0; i < num_predictions; ++i) {
        float best_score = 0.0f;
        int best_class = -1;

        for (int c = 0; c < num_classes; ++c) {
            const float score = output.at<float>(0, 4 + c, i);
            if (score > best_score) {
                best_score = score;
                best_class = c;
            }
        }

        if (best_class < 0 || best_score < conf_threshold_ || !is_target_class(best_class)) {
            continue;
        }

        const float cx = output.at<float>(0, 0, i);
        const float cy = output.at<float>(0, 1, i);
        const float w = output.at<float>(0, 2, i);
        const float h = output.at<float>(0, 3, i);

        const float cx_unpad = (cx - letterbox.pad_x) / letterbox.scale;
        const float cy_unpad = (cy - letterbox.pad_y) / letterbox.scale;
        const float w_unpad = w / letterbox.scale;
        const float h_unpad = h / letterbox.scale;

        Detection det;
        det.class_id = best_class;
        det.confidence = best_score;
        det.bbox.x = cx_unpad - w_unpad * 0.5f;
        det.bbox.y = cy_unpad - h_unpad * 0.5f;
        det.bbox.w = w_unpad;
        det.bbox.h = h_unpad;

        det.bbox.x = std::max(0.0f, std::min(det.bbox.x, static_cast<float>(frame_w - 1)));
        det.bbox.y = std::max(0.0f, std::min(det.bbox.y, static_cast<float>(frame_h - 1)));
        det.bbox.w = std::max(0.0f, std::min(det.bbox.w, static_cast<float>(frame_w) - det.bbox.x));
        det.bbox.h = std::max(0.0f, std::min(det.bbox.h, static_cast<float>(frame_h) - det.bbox.y));

        if (det.bbox.w > 0.0f && det.bbox.h > 0.0f) {
            detections.push_back(det);
        }
    }

    return nms_detections(detections, nms_iou_threshold_);
}

std::vector<Detection> YoloV8Detector::process(const Frame& frame) {
    if (frame.width <= 0 || frame.height <= 0 ||
        static_cast<size_t>(frame.width * frame.height * 3) != frame.data.size()) {
        return {};
    }

    cv::Mat rgb(frame.height, frame.width, CV_8UC3, const_cast<uint8_t*>(frame.data.data()));
    const LetterboxParams letterbox = compute_letterbox(frame.width, frame.height);

    cv::Mat resized;
    const int unpad_w = static_cast<int>(std::round(static_cast<float>(frame.width) * letterbox.scale));
    const int unpad_h = static_cast<int>(std::round(static_cast<float>(frame.height) * letterbox.scale));
    cv::resize(rgb, resized, cv::Size(unpad_w, unpad_h));

    cv::Mat letterboxed(kInputSize, kInputSize, CV_8UC3, cv::Scalar(114, 114, 114));
    const int pad_left = static_cast<int>(std::round(letterbox.pad_x));
    const int pad_top = static_cast<int>(std::round(letterbox.pad_y));
    resized.copyTo(letterboxed(cv::Rect(pad_left, pad_top, unpad_w, unpad_h)));

    cv::Mat blob =
        cv::dnn::blobFromImage(letterboxed, 1.0 / 255.0, cv::Size(), cv::Scalar(), false, false);
    net_.setInput(blob);

    std::vector<cv::Mat> outputs;
    net_.forward(outputs, net_.getUnconnectedOutLayersNames());
    if (outputs.empty()) {
        return {};
    }

    return decode_output(outputs[0], letterbox, frame.width, frame.height);
}

}  // namespace pipeline
