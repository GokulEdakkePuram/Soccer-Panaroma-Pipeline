#include "pipeline/stubs/PassthroughCamera.hpp"

#include "pipeline/DetectionClasses.hpp"

#include <algorithm>

namespace pipeline {
namespace {

// Smoothing factor
constexpr float kSmoothingAlpha = 0.2f;

}  // namespace

CameraTarget PassthroughCamera::update(int64_t /*frame_idx*/,
                                       const std::vector<Detection>& detections,
                                       const std::optional<CameraTarget>& prev_target,
                                       int frame_width,
                                       int frame_height) {
    // Select detections of person class and above confidence threshold, sum them up
    float sum_x = 0.0f;
    float sum_y = 0.0f;
    float sum_conf = 0.0f;
    int count = 0;
    for (const auto& det : detections) {
        if (det.class_id != kPersonClassId || det.confidence < kDefaultConfThreshold) {
            continue;
        }
        sum_x += det.bbox.x + det.bbox.w * 0.5f;
        sum_y += det.bbox.y + det.bbox.h * 0.5f;
        sum_conf += det.confidence;
        ++count;
    }

    CameraTarget target;
    if (count > 0) {
        // Centroid of the detected players.
        const float measured_x = sum_x / static_cast<float>(count);
        const float measured_y = sum_y / static_cast<float>(count);

        // Exponential smoothing against prev_target.
        if (prev_target.has_value()) {
            target.center_x =
                prev_target->center_x + kSmoothingAlpha * (measured_x - prev_target->center_x);
            target.center_y =
                prev_target->center_y + kSmoothingAlpha * (measured_y - prev_target->center_y);
        } else {
            target.center_x = measured_x;
            target.center_y = measured_y;
        }
        target.confidence = sum_conf / static_cast<float>(count);
    } else if (prev_target.has_value()) {
        // If no detections, use the previous target
        target = *prev_target;
    } else {
        target.center_x = static_cast<float>(frame_width) * 0.5f;
        target.center_y = static_cast<float>(frame_height) * 0.5f;
        target.confidence = 0.0f;
    }

    target.center_x = std::clamp(target.center_x, 0.0f, static_cast<float>(frame_width));
    target.center_y = std::clamp(target.center_y, 0.0f, static_cast<float>(frame_height));
    return target;
}

}  // namespace pipeline
