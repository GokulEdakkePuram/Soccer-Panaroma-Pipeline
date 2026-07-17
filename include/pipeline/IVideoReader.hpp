#pragma once

#include "pipeline/types.hpp"

namespace pipeline {

class IVideoReader {
public:
    virtual ~IVideoReader() = default;

    virtual bool open(const std::string& path, const Segment& segment) = 0;
    virtual std::optional<Frame> read_next() = 0;
    virtual bool seek(int64_t frame_idx) = 0;
    virtual VideoMetadata metadata() const = 0;
};

}  // namespace pipeline
