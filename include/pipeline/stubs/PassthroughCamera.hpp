#pragma once

#include "pipeline/ICamera.hpp"

namespace pipeline {

class PassthroughCamera : public ICamera {
public:
    CameraTarget update(int64_t frame_idx,
                        const std::vector<Detection>& detections,
                        const std::optional<CameraTarget>& prev_target,
                        int frame_width,
                        int frame_height) override;
};

}  // namespace pipeline
