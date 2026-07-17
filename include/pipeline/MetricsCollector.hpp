#pragma once

#include "pipeline/IMetrics.hpp"

#include <vector>

namespace pipeline {

class MetricsCollector : public IMetrics {
public:
    void record_frame(const FrameResult& result) override;
    void write_json(const std::string& output_path, const VideoMetadata& metadata) const override;
    std::vector<FrameResult> results() const override;

private:
    std::vector<FrameResult> results_;
};

}  // namespace pipeline
