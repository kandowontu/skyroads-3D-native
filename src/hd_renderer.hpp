#pragma once

#include <cstdint>
#include <vector>

namespace skyroads {

/*
 * Smooth high-definition presentation. Each four-pixel source cell is treated
 * as a smoothly shaded quad, preserving the recovered 320x200 simulation while
 * removing nearest-neighbour stair stepping at the native window resolution.
 */
void render_smooth_quads(
    const std::vector<std::uint32_t>& source,
    unsigned source_width,
    unsigned source_height,
    unsigned destination_width,
    unsigned destination_height,
    std::vector<std::uint32_t>& destination);

} // namespace skyroads
