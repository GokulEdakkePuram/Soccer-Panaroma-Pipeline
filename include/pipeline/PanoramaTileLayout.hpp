#pragma once

#include <vector>

namespace pipeline {

struct TileRegion {
    int x0 = 0;
    int y0 = 0;
    int w = 0;
    int h = 0;
};

inline std::vector<TileRegion> default_tile_layout(int panorama_width, int panorama_height,
                                                   int num_tiles = 5) {
    const int tile_width = panorama_width / num_tiles;
    const int tile_height = tile_width;
    const int y0 = (panorama_height - tile_height) / 2;

    std::vector<TileRegion> tiles;
    tiles.reserve(static_cast<size_t>(num_tiles));
    for (int i = 0; i < num_tiles; ++i) {
        tiles.push_back({i * tile_width, y0, tile_width, tile_height});
    }
    return tiles;
}

}  // namespace pipeline
