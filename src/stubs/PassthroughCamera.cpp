#include "pipeline/stubs/PassthroughCamera.hpp"

#include <algorithm>

namespace pipeline {

CameraTarget PassthroughCamera::update(int64_t /*frame_idx*/,
                                       const std::vector<Detection>& /*detections*/,
                                       const std::optional<CameraTarget>& prev_target,
                                       int frame_width,
                                       int frame_height) {
    CameraTarget target;
    target.center_x = static_cast<float>(frame_width) * 0.5f;
    target.center_y = static_cast<float>(frame_height) * 0.5f;
    target.confidence = prev_target.has_value() ? prev_target->confidence : 1.0f;
    return target;
}

}  // namespace pipeline
