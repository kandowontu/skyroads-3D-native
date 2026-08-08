#include "expanded_level_menu.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

namespace skyroads {
namespace {

constexpr unsigned kWidth = 320;
constexpr unsigned kHeight = 200;
constexpr unsigned kColumnWidth = 80;
constexpr unsigned kWorldBlockHeight = 39;
constexpr unsigned kThumbnailWidth = 28;
constexpr unsigned kThumbnailHeight = 18;
constexpr std::uint8_t kPanelBorder = 200;
constexpr std::uint8_t kSelector = 1;
constexpr std::uint8_t kRoadText = 2;
constexpr std::uint8_t kWorldText = 3;

constexpr std::array<std::string_view, 20> kWorldNames{
    "RED HEAT", "INTO THE SUN", "BLUE PLANET", "SATELLITE", "MISTY",
    "ASTEROID BLT", "CRAB NEBULA", "OVER BASE", "THE EARTH", "DROUDIA",
    "SNOWBOUND", "AT OUTER RIM", "TWILIGHT ZN", "GUIDING STAR", "METEOR STORM",
    "MYSTERIOUS", "NORTH LIGHTS", "OVER POLE", "UNDER ICE", "THE EVE",
};

std::array<std::uint8_t, 7> glyph(char character) {
    switch (character) {
    case 'A': return {14,17,17,31,17,17,17};
    case 'B': return {30,17,17,30,17,17,30};
    case 'C': return {14,17,16,16,16,17,14};
    case 'D': return {30,17,17,17,17,17,30};
    case 'E': return {31,16,16,30,16,16,31};
    case 'F': return {31,16,16,30,16,16,16};
    case 'G': return {14,17,16,23,17,17,15};
    case 'H': return {17,17,17,31,17,17,17};
    case 'I': return {14,4,4,4,4,4,14};
    case 'J': return {7,2,2,2,18,18,12};
    case 'K': return {17,18,20,24,20,18,17};
    case 'L': return {16,16,16,16,16,16,31};
    case 'M': return {17,27,21,21,17,17,17};
    case 'N': return {17,25,21,19,17,17,17};
    case 'O': return {14,17,17,17,17,17,14};
    case 'P': return {30,17,17,30,16,16,16};
    case 'Q': return {14,17,17,17,21,18,13};
    case 'R': return {30,17,17,30,20,18,17};
    case 'S': return {15,16,16,14,1,1,30};
    case 'T': return {31,4,4,4,4,4,4};
    case 'U': return {17,17,17,17,17,17,14};
    case 'V': return {17,17,17,17,17,10,4};
    case 'W': return {17,17,17,21,21,21,10};
    case 'X': return {17,17,10,4,10,17,17};
    case 'Y': return {17,17,10,4,4,4,4};
    case 'Z': return {31,1,2,4,8,16,31};
    case '0': return {14,17,19,21,25,17,14};
    case '1': return {4,12,4,4,4,4,14};
    case '2': return {14,17,1,2,4,8,31};
    case '3': return {30,1,1,14,1,1,30};
    case '4': return {2,6,10,18,31,2,2};
    case '5': return {31,16,16,30,1,1,30};
    case '6': return {14,16,16,30,17,17,14};
    case '7': return {31,1,2,4,8,8,8};
    case '8': return {14,17,17,14,17,17,14};
    case '9': return {14,17,17,15,1,1,14};
    case '-': return {0,0,0,31,0,0,0};
    default: return {};
    }
}

void pixel(std::vector<std::uint8_t>& framebuffer,
    unsigned x, unsigned y, std::uint8_t color) {
    if (x < kWidth && y < kHeight) framebuffer[y * kWidth + x] = color;
}

void draw_text(std::vector<std::uint8_t>& framebuffer,
    unsigned x, unsigned y, std::string_view text, std::uint8_t color) {
    for (const char character : text) {
        const auto rows = glyph(character);
        for (unsigned row = 0; row < rows.size(); ++row) {
            for (unsigned column = 0; column < 5; ++column) {
                if ((rows[row] & (1u << (4u - column))) != 0) {
                    pixel(framebuffer, x + column, y + row, color);
                }
            }
        }
        x += 6;
    }
}

void rectangle(std::vector<std::uint8_t>& framebuffer,
    unsigned x, unsigned y, unsigned width, unsigned height,
    std::uint8_t color) {
    if (width == 0 || height == 0) return;
    for (unsigned column = 0; column < width; ++column) {
        pixel(framebuffer, x + column, y, color);
        pixel(framebuffer, x + column, y + height - 1u, color);
    }
    for (unsigned row = 0; row < height; ++row) {
        pixel(framebuffer, x, y + row, color);
        pixel(framebuffer, x + width - 1u, y + row, color);
    }
}

void draw_world_thumbnail(
    std::vector<std::uint8_t>& framebuffer,
    const std::uint8_t* menu_art,
    unsigned campaign_world,
    unsigned destination_x,
    unsigned destination_y) {
    const unsigned source_x = campaign_world < 5u ? 8u : 168u;
    const unsigned source_y = 11u + (campaign_world % 5u) * kWorldBlockHeight;
    constexpr unsigned source_width = 52u;
    constexpr unsigned source_height = 28u;
    for (unsigned y = 0; y < kThumbnailHeight; ++y) {
        const unsigned sample_y = source_y + y * source_height / kThumbnailHeight;
        for (unsigned x = 0; x < kThumbnailWidth; ++x) {
            const unsigned sample_x = source_x + x * source_width / kThumbnailWidth;
            pixel(framebuffer, destination_x + x, destination_y + y,
                menu_art[sample_y * kWidth + sample_x]);
        }
    }
}

} // namespace

void draw_native_text(
    std::vector<std::uint8_t>& framebuffer,
    unsigned x,
    unsigned y,
    std::string_view text,
    std::uint8_t color) {
    draw_text(framebuffer, x, y, text, color);
}

void draw_native_rectangle(
    std::vector<std::uint8_t>& framebuffer,
    unsigned x,
    unsigned y,
    unsigned width,
    unsigned height,
    std::uint8_t color) {
    rectangle(framebuffer, x, y, width, height, color);
}

void fill_native_rectangle(
    std::vector<std::uint8_t>& framebuffer,
    unsigned x,
    unsigned y,
    unsigned width,
    unsigned height,
    std::uint8_t color) {
    for (unsigned row = 0; row < height; ++row) {
        for (unsigned column = 0; column < width; ++column) {
            pixel(framebuffer, x + column, y + row, color);
        }
    }
}

void draw_original_menu_backdrop(
    std::vector<std::uint8_t>& framebuffer,
    const std::uint8_t* original_menu_art) {
    if (framebuffer.size() != kWidth * kHeight || original_menu_art == nullptr) return;
    for (unsigned y = 0; y < kHeight; ++y) {
        for (unsigned x = 0; x < kWidth; ++x) {
            framebuffer[y * kWidth + x] =
                original_menu_art[y * kWidth + 116u + (x % 44u)];
        }
    }
}

SrLevelMenuAction expanded_level_menu_key(
    SrLevelMenuState& state,
    std::uint16_t key,
    unsigned level_count) {
    level_count = std::clamp(level_count, 1u, kCombinedLevelCount);
    if (state.current_level >= level_count) {
        state.current_level = static_cast<std::uint16_t>(level_count - 1u);
    }
    const unsigned row = state.current_level % kLevelsPerMenuColumn;
    switch (key) {
    case SR_MENU_KEY_ENTER:
        return SR_LEVEL_MENU_START;
    case SR_MENU_KEY_ESCAPE:
        return SR_LEVEL_MENU_BACK;
    case SR_MENU_KEY_UP:
        if (row != 0) --state.current_level;
        break;
    case SR_MENU_KEY_DOWN:
        if (row + 1u < kLevelsPerMenuColumn &&
            state.current_level + 1u < level_count) ++state.current_level;
        break;
    case SR_MENU_KEY_LEFT:
        if (state.current_level >= kLevelsPerMenuColumn) {
            state.current_level = static_cast<std::uint16_t>(
                state.current_level - kLevelsPerMenuColumn);
        }
        break;
    case SR_MENU_KEY_RIGHT:
        if (state.current_level + kLevelsPerMenuColumn < level_count) {
            state.current_level = static_cast<std::uint16_t>(
                state.current_level + kLevelsPerMenuColumn);
        }
        break;
    default:
        break;
    }
    return SR_LEVEL_MENU_NONE;
}

bool expanded_level_menu_layout(
    unsigned level,
    unsigned level_count,
    ExpandedLevelMenuLayout& layout) {
    if (level >= level_count || level_count == 0 ||
        level_count > kCombinedLevelCount) return false;
    const unsigned column = level / kLevelsPerMenuColumn;
    const unsigned row = level % kLevelsPerMenuColumn;
    const unsigned world = row / 3u;
    const unsigned road = row % 3u;
    layout.selector_x = column * kColumnWidth + 31u;
    layout.selector_y = world * kWorldBlockHeight + 10u + road * 9u;
    layout.selector_width = 48u;
    layout.selector_height = 8u;
    layout.completion_x = column * kColumnWidth + 71u;
    layout.completion_y = layout.selector_y + 2u;
    return true;
}

bool render_expanded_level_menu(
    std::vector<std::uint8_t>& framebuffer,
    const std::uint8_t* original_menu_art,
    const std::uint8_t* xmas_menu_art,
    const std::uint16_t* completion_count,
    unsigned level_count,
    unsigned selected_level) {
    if (framebuffer.size() != kWidth * kHeight || original_menu_art == nullptr ||
        completion_count == nullptr ||
        level_count == 0 || level_count > kCombinedLevelCount ||
        selected_level >= level_count ||
        (level_count > kOriginalLevelCount && xmas_menu_art == nullptr)) return false;

    /* The original menu's x=116..159 strip contains only its blue space
       backdrop. Tile those exact indexed pixels so the compact layout keeps
       the first game's background without duplicating its old two-column
       labels and thumbnails. */
    draw_original_menu_backdrop(framebuffer, original_menu_art);
    for (unsigned column = 0; column < 4u; ++column) {
        if (column != 0) {
            for (unsigned y = 0; y < kHeight; ++y) {
                pixel(framebuffer, column * kColumnWidth, y, kPanelBorder);
            }
        }
    }

    const unsigned world_count = (level_count + 2u) / 3u;
    for (unsigned world = 0; world < world_count; ++world) {
        const unsigned campaign_world = world < 10u ? world : world - 10u;
        const unsigned column = world < 10u
            ? world / 5u : 2u + campaign_world / 5u;
        const unsigned block = campaign_world % 5u;
        const unsigned x = column * kColumnWidth + 3u;
        const unsigned y = block * kWorldBlockHeight + 1u;
        draw_text(framebuffer, x, y, kWorldNames[world], kWorldText);
        draw_world_thumbnail(
            framebuffer,
            world < 10u ? original_menu_art : xmas_menu_art,
            campaign_world,
            column * kColumnWidth + 2u,
            block * kWorldBlockHeight + 10u);
        for (unsigned road = 0; road < 3u; ++road) {
            const unsigned level = world * 3u + road;
            if (level >= level_count) break;
            const unsigned road_y = block * kWorldBlockHeight + 10u + road * 9u;
            draw_text(framebuffer, column * kColumnWidth + 33u, road_y,
                std::string("ROAD ") + static_cast<char>('1' + road), kRoadText);
            const char completions = static_cast<char>('0' +
                std::min<unsigned>(completion_count[level], 9u));
            draw_text(framebuffer, column * kColumnWidth + 72u, road_y,
                std::string_view(&completions, 1u),
                kExpandedCompletionTextColor);
        }
    }

    ExpandedLevelMenuLayout selected{};
    if (!expanded_level_menu_layout(selected_level, level_count, selected)) return false;
    rectangle(framebuffer, selected.selector_x, selected.selector_y,
        selected.selector_width, selected.selector_height, kSelector);
    return true;
}

} // namespace skyroads
