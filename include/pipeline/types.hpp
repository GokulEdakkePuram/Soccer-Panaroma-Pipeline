#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace pipeline {

struct BoundingBox {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

struct Frame {
    int64_t frame_idx = 0;
    double timestamp_ms = 0.0;
    int width = 0;
    int height = 0;
    std::vector<uint8_t> data;  // RGB, row-major
};

struct Detection {
    int class_id = 0;
    float confidence = 0.0f;
    BoundingBox bbox;
};

struct CameraTarget {
    float center_x = 0.0f;
    float center_y = 0.0f;
    float confidence = 0.0f;
};

struct Segment {
    int64_t start_frame = 0;
    int64_t end_frame = -1;  // -1 means until EOF
};

struct StageTimingMs {
    double video_reader = 0.0;
    double detector = 0.0;
    double camera = 0.0;
    double visualizer = 0.0;
};

struct FrameResult {
    int64_t frame_idx = 0;
    double timestamp_ms = 0.0;
    CameraTarget camera_center;
    StageTimingMs stage_ms;
};

struct PipelineConfig {
    std::string video_path;
    std::string output_path;
    std::string viz_output_path;
    std::string model_path = "models/yolov8n.onnx";
    Segment segment;
    int queue_depth = 8;
    bool realtime = false;
};

struct VideoMetadata {
    std::string video_path;
    int width = 0;
    int height = 0;
    double fps = 25.0;
    int64_t total_frames = 0;
    Segment segment;
};

}  // namespace pipeline
