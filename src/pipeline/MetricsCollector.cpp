#include "pipeline/MetricsCollector.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <numeric>

namespace pipeline {
namespace {

double percentile(std::vector<double> values, double p) {
    if (values.empty()) {
        return 0.0;
    }
    const size_t idx = static_cast<size_t>(p * (values.size() - 1));
    std::nth_element(values.begin(), values.begin() + static_cast<long>(idx), values.end());
    return values[idx];
}

StageTimingMs average_stage_timing(const std::vector<FrameResult>& results) {
    StageTimingMs avg{};
    if (results.empty()) {
        return avg;
    }
    for (const auto& r : results) {
        avg.video_reader += r.stage_ms.video_reader;
        avg.detector += r.stage_ms.detector;
        avg.camera += r.stage_ms.camera;
        avg.visualizer += r.stage_ms.visualizer;
    }
    const double n = static_cast<double>(results.size());
    avg.video_reader /= n;
    avg.detector /= n;
    avg.camera /= n;
    avg.visualizer /= n;
    return avg;
}

StageTimingMs p95_stage_timing(const std::vector<FrameResult>& results) {
    StageTimingMs p95{};
    if (results.empty()) {
        return p95;
    }
    std::vector<double> vr, det, cam, vis;
    vr.reserve(results.size());
    det.reserve(results.size());
    cam.reserve(results.size());
    vis.reserve(results.size());
    for (const auto& r : results) {
        vr.push_back(r.stage_ms.video_reader);
        det.push_back(r.stage_ms.detector);
        cam.push_back(r.stage_ms.camera);
        vis.push_back(r.stage_ms.visualizer);
    }
    p95.video_reader = percentile(vr, 0.95);
    p95.detector = percentile(det, 0.95);
    p95.camera = percentile(cam, 0.95);
    p95.visualizer = percentile(vis, 0.95);
    return p95;
}

}  // namespace

void MetricsCollector::record_frame(const FrameResult& result) {
    results_.push_back(result);
}

std::vector<FrameResult> MetricsCollector::results() const {
    return results_;
}

void MetricsCollector::write_json(const std::string& output_path,
                                  const VideoMetadata& metadata) const {
    nlohmann::json doc;
    doc["metadata"] = {
        {"video", metadata.video_path},
        {"width", metadata.width},
        {"height", metadata.height},
        {"fps", metadata.fps},
        {"segment", {{"start_frame", metadata.segment.start_frame},
                      {"end_frame", metadata.segment.end_frame}}}};

    nlohmann::json frames = nlohmann::json::array();
    for (const auto& r : results_) {
        frames.push_back({
            {"frame_idx", r.frame_idx},
            {"timestamp_ms", r.timestamp_ms},
            {"camera_center", {{"x", r.camera_center.center_x}, {"y", r.camera_center.center_y}}},
            {"stage_ms",
             {{"video_reader", r.stage_ms.video_reader},
              {"detector", r.stage_ms.detector},
              {"camera", r.stage_ms.camera},
              {"visualizer", r.stage_ms.visualizer}}},
        });
    }
    doc["frames"] = frames;

    const auto avg = average_stage_timing(results_);
    const double total_stage_ms = avg.video_reader + avg.detector + avg.camera + avg.visualizer;
    const double avg_fps = total_stage_ms > 0.0 ? 1000.0 / total_stage_ms : 0.0;
    const auto p95 = p95_stage_timing(results_);

    doc["summary"] = {
        {"frames_processed", results_.size()},
        {"avg_fps", avg_fps},
        {"stage_ms_avg",
         {{"video_reader", avg.video_reader},
          {"detector", avg.detector},
          {"camera", avg.camera},
          {"visualizer", avg.visualizer}}},
        {"stage_ms_p95",
         {{"video_reader", p95.video_reader},
          {"detector", p95.detector},
          {"camera", p95.camera},
          {"visualizer", p95.visualizer}}},
    };

    std::ofstream out(output_path);
    out << doc.dump(2) << '\n';
}

}  // namespace pipeline
