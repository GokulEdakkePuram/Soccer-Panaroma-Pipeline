#pragma once

#include "pipeline/IMetrics.hpp"

#include <string>
#include <vector>

namespace pipeline
{

class MetricsCollector : public IMetrics
{
  public:
    void record_frame(const FrameResult &result) override;
    void record_error(const std::string &stage, const std::string &message) override;
    void write_json(const std::string &output_path, const VideoMetadata &metadata) const override;
    std::vector<FrameResult> results() const override;

  private:
    std::vector<FrameResult> results_;
    std::string error_stage_;
    std::string error_message_;
};

} // namespace pipeline
