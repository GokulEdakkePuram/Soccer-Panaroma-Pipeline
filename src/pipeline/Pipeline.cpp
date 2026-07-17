#include "pipeline/Pipeline.hpp"

#include <chrono>
#include <iostream>
#include <thread>

namespace pipeline {
namespace {

using Clock = std::chrono::steady_clock;

double elapsed_ms(const Clock::time_point& start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

}  // namespace

Pipeline::Pipeline(std::unique_ptr<IVideoReader> reader,
                   std::unique_ptr<IDetector> detector,
                   std::unique_ptr<ICamera> camera,
                   std::unique_ptr<IVisualizer> visualizer,
                   std::unique_ptr<IMetrics> metrics,
                   PipelineConfig config)
    : reader_(std::move(reader)),
      detector_(std::move(detector)),
      camera_(std::move(camera)),
      visualizer_(std::move(visualizer)),
      metrics_(std::move(metrics)),
      config_(std::move(config)) {}

bool Pipeline::run() {
    if (!reader_->open(config_.video_path, config_.segment)) {
        std::cerr << "Failed to open video reader\n";
        return false;
    }

    metadata_ = reader_->metadata();
    if (metadata_.segment.end_frame < 0 && metadata_.total_frames > 0) {
        metadata_.segment.end_frame = metadata_.total_frames - 1;
    }

    if (!config_.viz_output_path.empty() && visualizer_) {
        if (!visualizer_->open(config_.viz_output_path, metadata_.width, metadata_.height,
                               metadata_.fps)) {
            std::cerr << "Failed to open visualizer output\n";
            return false;
        }
    }

    const double frame_budget_ms = metadata_.fps > 0.0 ? 1000.0 / metadata_.fps : 40.0;

    // Baseline: every stage runs on this single thread, one frame at a time.
    // config_.queue_depth is unused here — your concurrent implementation should
    // use it as the bounded queue capacity between stages.
    while (true) {
        const auto frame_start = Clock::now();

        const auto reader_t0 = Clock::now();
        auto frame = reader_->read_next();
        const double reader_ms = elapsed_ms(reader_t0);

        if (!frame.has_value()) {
            break;
        }

        const auto detector_t0 = Clock::now();
        auto detections = detector_->process(*frame);
        const double detector_ms = elapsed_ms(detector_t0);

        const auto camera_t0 = Clock::now();
        CameraTarget target = camera_->update(frame->frame_idx, detections, prev_target_,
                                              frame->width, frame->height);
        const double camera_ms = elapsed_ms(camera_t0);
        prev_target_ = target;

        double visualizer_ms = 0.0;
        if (visualizer_) {
            const auto viz_t0 = Clock::now();
            visualizer_->render(*frame, detections, target);
            visualizer_ms = elapsed_ms(viz_t0);
        }

        FrameResult result;
        result.frame_idx = frame->frame_idx;
        result.timestamp_ms = frame->timestamp_ms;
        result.camera_center = target;
        result.stage_ms.video_reader = reader_ms;
        result.stage_ms.detector = detector_ms;
        result.stage_ms.camera = camera_ms;
        result.stage_ms.visualizer = visualizer_ms;
        metrics_->record_frame(result);

        if (config_.realtime) {
            const double elapsed = elapsed_ms(frame_start);
            if (elapsed < frame_budget_ms) {
                std::this_thread::sleep_for(
                    std::chrono::duration<double, std::milli>(frame_budget_ms - elapsed));
            }
        }
    }

    if (visualizer_) {
        visualizer_->close();
    }

    metrics_->write_json(config_.output_path, metadata_);
    return true;
}

}  // namespace pipeline
