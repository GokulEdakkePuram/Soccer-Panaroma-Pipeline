#include "pipeline/stubs/NullDetector.hpp"

namespace pipeline {

std::vector<Detection> NullDetector::process(const Frame& /*frame*/) {
    return {};
}

}  // namespace pipeline
