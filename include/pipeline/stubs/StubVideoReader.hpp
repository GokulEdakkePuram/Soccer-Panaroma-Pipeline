#pragma once

#include "pipeline/IVideoReader.hpp"

namespace pipeline {

// Stub video reader — YOUR TASK: replace this with a real implementation.
//
// Requirements (see TASK.md, Task 1):
//   - Stream frames one at a time (never load the whole video into memory)
//   - Honor segment.start_frame / segment.end_frame, including seeking
//   - Fill Frame with frame_idx, timestamp_ms, dimensions, and RGB pixel data
//   - Populate VideoMetadata (width, height, fps, total_frames)
//
// Any backend is fine: raw FFmpeg (libav* is already linked) or OpenCV
// cv::VideoCapture (OpenCV is already a dependency).
class StubVideoReader : public IVideoReader {
public:
    bool open(const std::string& path, const Segment& segment) override;
    std::optional<Frame> read_next() override;
    bool seek(int64_t frame_idx) override;
    VideoMetadata metadata() const override;

private:
    VideoMetadata metadata_;
};

}  // namespace pipeline
