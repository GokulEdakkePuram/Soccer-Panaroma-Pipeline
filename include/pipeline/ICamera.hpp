#pragma once

#include "pipeline/types.hpp"

#include <optional>
#include <vector>

namespace pipeline {

class ICamera {
public:
    virtual ~ICamera() = default;

    virtual CameraTarget update(int64_t frame_idx,
                                const std::vector<Detection>& detections,
                                const std::optional<CameraTarget>& prev_target,
                                int frame_width,
                                int frame_height) = 0;
};

}  // namespace pipeline
