#pragma once

#include "pipeline/types.hpp"

#include <string>
#include <vector>

namespace pipeline
{

class IMetrics
{
  public:
    virtual ~IMetrics() = default;

    virtual void record_frame(const FrameResult &result) = 0;
    // Records why the run stopped early; write_json() then reports it.
    virtual void record_error(const std::string &stage, const std::string &message) = 0;
    virtual void write_json(const std::string &output_path, const VideoMetadata &metadata) const = 0;
    virtual std::vector<FrameResult> results() const = 0;
};

} // namespace pipeline
