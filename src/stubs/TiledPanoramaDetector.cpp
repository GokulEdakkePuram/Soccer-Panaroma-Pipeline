#include "pipeline/stubs/TiledPanoramaDetector.hpp"

#include "pipeline/PanoramaTileLayout.hpp"

namespace pipeline {

TiledPanoramaDetector::TiledPanoramaDetector(std::unique_ptr<IDetector> tile_detector)
    : tile_detector_(std::move(tile_detector)) {}

std::vector<Detection> TiledPanoramaDetector::process(const Frame& frame) {
    // TODO: split panorama into tiles (see TASK.md)
    // TODO: run tile_detector_->process() on each tile — per-tile post-processing is already done
    // TODO: merge post-processing — offset bboxes to panorama coords, cross-tile NMS, sort

    // Scaffold: center tile only so partial detections appear before full implementation.
    const auto tiles = default_tile_layout(frame.width, frame.height);
    if (tiles.size() < 3) {
        return {};
    }

    const TileRegion& center = tiles[2];
    Frame tile;
    tile.frame_idx = frame.frame_idx;
    tile.timestamp_ms = frame.timestamp_ms;
    tile.width = center.w;
    tile.height = center.h;
    tile.data.resize(static_cast<size_t>(center.w * center.h * 3));

    for (int y = 0; y < center.h; ++y) {
        const int src_y = center.y0 + y;
        const size_t src_offset =
            static_cast<size_t>((src_y * frame.width + center.x0) * 3);
        const size_t dst_offset = static_cast<size_t>(y * center.w * 3);
        std::copy(frame.data.begin() + static_cast<std::ptrdiff_t>(src_offset),
                  frame.data.begin() + static_cast<std::ptrdiff_t>(src_offset + center.w * 3),
                  tile.data.begin() + static_cast<std::ptrdiff_t>(dst_offset));
    }

    auto detections = tile_detector_->process(tile);
    for (auto& det : detections) {
        det.bbox.x += static_cast<float>(center.x0);
        det.bbox.y += static_cast<float>(center.y0);
    }
    return detections;
}

}  // namespace pipeline
