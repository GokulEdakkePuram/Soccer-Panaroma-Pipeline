#pragma once

#include "pipeline/IVisualizer.hpp"

namespace pipeline {

class NullVisualizer : public IVisualizer {
public:
    bool open(const std::string& output_path, int width, int height, double fps) override;
    void render(const Frame& frame,
                const std::vector<Detection>& detections,
                const CameraTarget& target) override;
    void close() override;
};

}  // namespace pipeline
