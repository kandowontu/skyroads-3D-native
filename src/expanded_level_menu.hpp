#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

extern "C" {
#include "menus.h"
}

namespace skyroads {

inline constexpr unsigned kOriginalLevelCount = 30;
inline constexpr unsigned kXmasLevelCount = 30;
inline constexpr unsigned kCombinedLevelCount =
    kOriginalLevelCount + kXmasLevelCount;
inline constexpr unsigned kLevelsPerMenuColumn = 15;
inline constexpr std::uint8_t kExpandedCompletionTextColor = 0xffu;

struct ExpandedLevelMenuLayout {
    unsigned selector_x{};
    unsigned selector_y{};
    unsigned selector_width{};
    unsigned selector_height{};
    unsigned completion_x{};
    unsigned completion_y{};
};

SrLevelMenuAction expanded_level_menu_key(
    SrLevelMenuState& state,
    std::uint16_t key,
    unsigned level_count);

bool expanded_level_menu_layout(
    unsigned level,
    unsigned level_count,
    ExpandedLevelMenuLayout& layout);

/* Port extension: compact four-column 320x200 selector for the two original
   30-road campaigns. It uses the base GOMENU palette indices and displays a
   saturated yellow 0-9 completion count beside every road. */
bool render_expanded_level_menu(
    std::vector<std::uint8_t>& framebuffer,
    const std::uint8_t* original_menu_art,
    const std::uint8_t* xmas_menu_art,
    const std::uint16_t* completion_count,
    unsigned level_count,
    unsigned selected_level);

/* Shared native-extension drawing primitives.  They intentionally use the
   compact VGA-style alphabet and indexed colors established by the expanded
   original level selector. */
void draw_native_text(
    std::vector<std::uint8_t>& framebuffer,
    unsigned x,
    unsigned y,
    std::string_view text,
    std::uint8_t color);
void draw_native_text_scaled(
    std::vector<std::uint8_t>& framebuffer,
    unsigned x,
    unsigned y,
    std::string_view text,
    std::uint8_t color,
    unsigned scale);
void draw_native_rectangle(
    std::vector<std::uint8_t>& framebuffer,
    unsigned x,
    unsigned y,
    unsigned width,
    unsigned height,
    std::uint8_t color);
void fill_native_rectangle(
    std::vector<std::uint8_t>& framebuffer,
    unsigned x,
    unsigned y,
    unsigned width,
    unsigned height,
    std::uint8_t color);
void draw_original_menu_backdrop(
    std::vector<std::uint8_t>& framebuffer,
    const std::uint8_t* original_menu_art);

} // namespace skyroads
