#pragma once

#include <cstdint>
#include <span>

namespace skyroads {

inline constexpr unsigned kKosmonautScreenWidth = 320u;
inline constexpr unsigned kKosmonautScreenHeight = 200u;
inline constexpr unsigned kKosmonautScreenCount = 5u;
inline constexpr unsigned kKosmonautRoadCount = 26u;
inline constexpr unsigned kKosmonautRoadRows = 200u;
inline constexpr unsigned kKosmonautRoadColumns = 6u;

std::span<const std::uint8_t> kosmonaut_screens();
std::span<const std::uint8_t> kosmonaut_roads();
std::span<const std::uint8_t> kosmonaut_demo_road();
std::span<const std::uint8_t> kosmonaut_tutorial_road();
std::span<const std::uint8_t> kosmonaut_font();
std::span<const std::uint8_t> kosmonaut_font_styles();
std::span<const std::uint8_t> kosmonaut_music();
std::span<const std::uint8_t> kosmonaut_star_motion();
std::span<const std::uint8_t> kosmonaut_star_rng();
std::span<const std::uint8_t> kosmonaut_star_path_pointers();
std::span<const std::uint8_t> kosmonaut_star_paths();
std::span<const std::uint8_t> kosmonaut_level_effects();
std::span<const std::uint8_t> kosmonaut_demo_effects();
std::span<const std::uint8_t> kosmonaut_demo_input();
std::span<const std::uint8_t> kosmonaut_palette_registers();
std::span<const std::uint8_t> kosmonaut_ship_sprites();
std::span<const std::uint8_t> kosmonaut_render_masks();
std::span<const std::uint8_t> kosmonaut_render_pointers();
std::span<const std::uint8_t> kosmonaut_render_shapes();
std::span<const std::uint8_t> kosmonaut_simulation_tables();
std::span<const std::uint8_t> kosmonaut_title_scroll();
std::span<const std::uint8_t> kosmonaut_tutorial_scroll();
std::span<const std::uint8_t> kosmonaut_destruction_frames();

} // namespace skyroads
