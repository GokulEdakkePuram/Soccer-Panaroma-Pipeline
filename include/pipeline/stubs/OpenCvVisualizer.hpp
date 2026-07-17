#pragma once

#include "pipeline/IVisualizer.hpp"

#include <memory>

namespace pipeline {

class OpenCvVisualizer : public IVisualizer {
public:
    OpenCvVisualizer();
    ~OpenCvVisualizer() override;
    bool open(const std::string& output_path, int width, int height, double fps) override;
    void render(const Frame& frame,
                const std::vector<Detection>& detections,
                const CameraTarget& target) override;
    void close() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace pipeline
