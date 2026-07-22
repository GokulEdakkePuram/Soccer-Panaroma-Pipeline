#include "pipeline/stubs/TiledPanoramaDetector.hpp"

#include "pipeline/PanoramaTileLayout.hpp"
#include "pipeline/YoloPostprocess.hpp"

#include <algorithm>

namespace pipeline
{
namespace
{

Frame extract_tile(const Frame &frame, const TileRegion &tile)
{
    Frame out;
    out.frame_idx = frame.frame_idx;
    out.timestamp_ms = frame.timestamp_ms;
    out.width = tile.w;
    out.height = tile.h;
    out.data.resize(static_cast<size_t>(tile.w * tile.h * 3));

    for (int y = 0; y < tile.h; ++y)
    {
        const int src_y = tile.y0 + y;
        const size_t src_offset = static_cast<size_t>((src_y * frame.width + tile.x0) * 3);
        const size_t dst_offset = static_cast<size_t>(y * tile.w * 3);
        std::copy(frame.data.begin() + static_cast<std::ptrdiff_t>(src_offset),
                  frame.data.begin() + static_cast<std::ptrdiff_t>(src_offset + tile.w * 3),
                  out.data.begin() + static_cast<std::ptrdiff_t>(dst_offset));
    }
    return out;
}

} // namespace

TiledPanoramaDetector::TiledPanoramaDetector(std::unique_ptr<IDetector> tile_detector)
    : tile_detector_(std::move(tile_detector))
{
}

std::vector<Detection> TiledPanoramaDetector::process(const Frame &frame)
{
    const auto tiles = default_tile_layout(frame.width, frame.height);

    std::vector<Detection> merged;
    for (const auto &tile : tiles)
    {
        const Frame cropped = extract_tile(frame, tile);
        auto detections = tile_detector_->process(cropped);
        for (auto &det : detections)
        {
            det.bbox.x += static_cast<float>(tile.x0);
            det.bbox.y += static_cast<float>(tile.y0);
        }
        merged.insert(merged.end(), detections.begin(), detections.end());
    }
    return nms_detections(merged);
}

} // namespace pipeline
