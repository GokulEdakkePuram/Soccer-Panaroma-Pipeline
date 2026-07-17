#include "pipeline/stubs/StubVideoReader.hpp"

#include <opencv2/imgproc.hpp>

#include <iostream>

namespace pipeline {

bool StubVideoReader::open(const std::string& path, const Segment& segment) {
    if (!capture_.open(path, cv::CAP_FFMPEG)) {
        std::cerr << "StubVideoReader: failed to open video: " << path << '\n';
        return false;
    }

    metadata_ = VideoMetadata{};
    metadata_.video_path = path;
    metadata_.width = static_cast<int>(capture_.get(cv::CAP_PROP_FRAME_WIDTH));
    metadata_.height = static_cast<int>(capture_.get(cv::CAP_PROP_FRAME_HEIGHT));
    const double fps = capture_.get(cv::CAP_PROP_FPS);
    if (fps > 0.0) {
        metadata_.fps = fps;
    }
    metadata_.total_frames = static_cast<int64_t>(capture_.get(cv::CAP_PROP_FRAME_COUNT));
    metadata_.segment = segment;

    next_frame_idx_ = 0;
    if (segment.start_frame > 0 && !seek(segment.start_frame)) {
        std::cerr << "StubVideoReader: failed to seek to start frame " << segment.start_frame
                  << '\n';
        return false;
    }

    return true;
}

std::optional<Frame> StubVideoReader::read_next() {
    if (!capture_.isOpened()) {
        return std::nullopt;
    }

    const int64_t end_frame = metadata_.segment.end_frame;
    if (end_frame >= 0 && next_frame_idx_ > end_frame) {
        return std::nullopt;
    }

    cv::Mat bgr;
    if (!capture_.read(bgr) || bgr.empty()) {
        return std::nullopt;
    }

    Frame frame;
    frame.frame_idx = next_frame_idx_;
    frame.width = bgr.cols;
    frame.height = bgr.rows;
    frame.timestamp_ms = metadata_.fps > 0.0
                              ? static_cast<double>(frame.frame_idx) * 1000.0 / metadata_.fps
                              : 0.0;

    cv::Mat rgb;
    cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);
    if (!rgb.isContinuous()) {
        rgb = rgb.clone();
    }
    frame.data.assign(rgb.data, rgb.data + rgb.total() * rgb.elemSize());

    ++next_frame_idx_;
    return frame;
}

bool StubVideoReader::seek(int64_t frame_idx) {
    if (!capture_.isOpened() || frame_idx < 0) {
        return false;
    }
    if (!capture_.set(cv::CAP_PROP_POS_FRAMES, static_cast<double>(frame_idx))) {
        return false;
    }
    next_frame_idx_ = frame_idx;
    return true;
}

VideoMetadata StubVideoReader::metadata() const {
    return metadata_;
}

}  // namespace pipeline
