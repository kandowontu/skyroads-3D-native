#pragma once
#include "hd_renderer.hpp"
extern "C" {
#include "trek_archive.h"
}

namespace skyroads {
// Complete cell solids. The DOS scanline programs supply depth calibration,
// never a viewport boundary or a polygon silhouette.
void build_wide_road_mesh(WideRoadScene& scene, const SrTrekArchive& trek,
    const std::uint16_t* cells, std::size_t row_count, std::uint32_t distance);
}
