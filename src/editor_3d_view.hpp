#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "custom_levels.hpp"

namespace skyroads {

enum class EditorViewMode : std::uint8_t {
    Top,
    IsometricLeft,
    Straight,
    IsometricRight
};

[[nodiscard]] EditorViewMode next_editor_view(EditorViewMode mode);
[[nodiscard]] std::string_view editor_view_name(EditorViewMode mode);
[[nodiscard]] std::uint8_t editor_material_color(unsigned material);

/* Draws the 15-row editor page into x=0..202, y=31..181.  Top mode is kept
   by the original grid renderer and therefore returns false here. */
bool render_editor_spatial_view(
    std::vector<std::uint8_t>& framebuffer,
    const CustomLevel& level,
    std::size_t first_row,
    std::size_t selected_row,
    std::size_t selected_column,
    EditorViewMode mode);

} // namespace skyroads
