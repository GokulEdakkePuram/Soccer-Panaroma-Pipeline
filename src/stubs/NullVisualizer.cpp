#include "pipeline/stubs/NullVisualizer.hpp"

namespace pipeline {

bool NullVisualizer::open(const std::string& /*output_path*/,
                          int /*width*/,
                          int /*height*/,
                          double /*fps*/) {
    return true;
}

void NullVisualizer::render(const Frame& /*frame*/,
                            const std::vector<Detection>& /*detections*/,
                            const CameraTarget& /*target*/) {}

void NullVisualizer::close() {}

}  // namespace pipeline
