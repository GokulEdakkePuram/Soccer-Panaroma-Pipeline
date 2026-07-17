#pragma once

#include "pipeline/IDetector.hpp"

namespace pipeline {

class NullDetector : public IDetector {
public:
    std::vector<Detection> process(const Frame& frame) override;
};

}  // namespace pipeline
