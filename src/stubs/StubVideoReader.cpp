#include "pipeline/stubs/StubVideoReader.hpp"

#include <iostream>

namespace pipeline {

bool StubVideoReader::open(const std::string& path, const Segment& segment) {
    metadata_.video_path = path;
    metadata_.segment = segment;

    std::cerr << "StubVideoReader: no frames will be produced.\n"
              << "Implement a real IVideoReader (Task 1 in TASK.md) and wire it in main.cpp.\n";
    return true;
}

std::optional<Frame> StubVideoReader::read_next() {
    // TODO: decode and return the next frame within the configured segment.
    return std::nullopt;
}

bool StubVideoReader::seek(int64_t /*frame_idx*/) {
    // TODO: seek so the next read_next() returns the requested frame.
    return false;
}

VideoMetadata StubVideoReader::metadata() const {
    return metadata_;
}

}  // namespace pipeline
