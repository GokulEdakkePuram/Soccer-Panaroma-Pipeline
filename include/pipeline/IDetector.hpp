#pragma once

#include "pipeline/types.hpp"

#include <vector>

namespace pipeline {

class IDetector {
public:
    virtual ~IDetector() = default;

    virtual std::vector<Detection> process(const Frame& frame) = 0;
};

}  // namespace pipeline
