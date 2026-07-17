#pragma once

#include "pipeline/ICamera.hpp"
#include "pipeline/IDetector.hpp"
#include "pipeline/IMetrics.hpp"
#include "pipeline/IVideoReader.hpp"
#include "pipeline/IVisualizer.hpp"
#include "pipeline/types.hpp"

#include <memory>
#include <optional>

namespace pipeline {

// Sequential baseline pipeline: reads, detects, updates the camera, visualizes,
// and records metrics one frame at a time on a single thread.
//
// This is a STARTING POINT, not the expected final architecture. Your task is to
// re-architect it into a streaming, concurrent pipeline (see TASK.md): stages
// running in parallel, bounded queues between them, and backpressure so memory
// stays flat regardless of video length. Keep the constructor and run() contract
// so main.cpp and the eval harness continue to work.
class Pipeline {
public:
    Pipeline(std::unique_ptr<IVideoReader> reader,
             std::unique_ptr<IDetector> detector,
             std::unique_ptr<ICamera> camera,
             std::unique_ptr<IVisualizer> visualizer,
             std::unique_ptr<IMetrics> metrics,
             PipelineConfig config);

    bool run();

private:
    std::unique_ptr<IVideoReader> reader_;
    std::unique_ptr<IDetector> detector_;
    std::unique_ptr<ICamera> camera_;
    std::unique_ptr<IVisualizer> visualizer_;
    std::unique_ptr<IMetrics> metrics_;
    PipelineConfig config_;
    VideoMetadata metadata_;

    std::optional<CameraTarget> prev_target_;
};

}  // namespace pipeline
