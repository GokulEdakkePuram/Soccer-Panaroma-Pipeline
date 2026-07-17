#pragma once

#include "pipeline/types.hpp"

#include <vector>

namespace pipeline {

class IVisualizer {
public:
    virtual ~IVisualizer() = default;

    virtual bool open(const std::string& output_path, int width, int height, double fps) = 0;
    virtual void render(const Frame& frame,
                        const std::vector<Detection>& detections,
                        const CameraTarget& target) = 0;
    virtual void close() = 0;
};

}  // namespace pipeline
