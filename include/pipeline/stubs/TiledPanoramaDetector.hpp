#pragma once

#include "pipeline/IDetector.hpp"

#include <memory>

namespace pipeline {

class TiledPanoramaDetector : public IDetector {
public:
    explicit TiledPanoramaDetector(std::unique_ptr<IDetector> tile_detector);

    std::vector<Detection> process(const Frame& frame) override;

private:
    std::unique_ptr<IDetector> tile_detector_;
};

}  // namespace pipeline
