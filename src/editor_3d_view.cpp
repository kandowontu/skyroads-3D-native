#include "editor_3d_view.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace skyroads {
namespace {

constexpr int kScreenWidth = 320;
constexpr int kScreenHeight = 200;
constexpr int kViewportRight = 202;
constexpr int kViewportTop = 22;
constexpr int kViewportBottom = 181;
constexpr std::size_t kVisibleRows = 15u;
constexpr std::uint8_t kSelectionColor = 1u;
constexpr std::uint8_t kGridColor = 200u;

struct Point {
    int x;
    int y;
};

using Quad = std::array<Point, 4>;

void pixel(
    std::vector<std::uint8_t>& framebuffer,
    int x,
    int y,
    std::uint8_t color) {
    if (x >= 0 && x <= kViewportRight && y >= kViewportTop &&
        y <= kViewportBottom) {
        framebuffer[static_cast<std::size_t>(y) * kScreenWidth +
            static_cast<unsigned>(x)] = color;
    }
}

void line(
    std::vector<std::uint8_t>& framebuffer,
    Point from,
    Point to,
    std::uint8_t color) {
    const int delta_x = std::abs(to.x - from.x);
    const int step_x = from.x < to.x ? 1 : -1;
    const int delta_y = -std::abs(to.y - from.y);
    const int step_y = from.y < to.y ? 1 : -1;
    int error = delta_x + delta_y;
    for (;;) {
        pixel(framebuffer, from.x, from.y, color);
        if (from.x == to.x && from.y == to.y) break;
        const int doubled = error * 2;
        if (doubled >= delta_y) {
            error += delta_y;
            from.x += step_x;
        }
        if (doubled <= delta_x) {
            error += delta_x;
            from.y += step_y;
        }
    }
}

void outline(
    std::vector<std::uint8_t>& framebuffer,
    const Quad& quad,
    std::uint8_t color) {
    for (std::size_t edge = 0; edge < quad.size(); ++edge) {
        line(framebuffer, quad[edge], quad[(edge + 1u) % quad.size()], color);
    }
}

void fill(
    std::vector<std::uint8_t>& framebuffer,
    const Quad& quad,
    std::uint8_t color,
    unsigned darken_period = 0u) {
    int minimum_y = quad[0].y;
    int maximum_y = quad[0].y;
    for (const auto point : quad) {
        minimum_y = std::min(minimum_y, point.y);
        maximum_y = std::max(maximum_y, point.y);
    }
    minimum_y = std::max(minimum_y, kViewportTop);
    maximum_y = std::min(maximum_y, kViewportBottom);
    for (int y = minimum_y; y <= maximum_y; ++y) {
        std::array<int, 4> intersections{};
        std::size_t count = 0u;
        for (std::size_t edge = 0; edge < quad.size(); ++edge) {
            const auto first = quad[edge];
            const auto second = quad[(edge + 1u) % quad.size()];
            if (first.y == second.y ||
                y < std::min(first.y, second.y) ||
                y >= std::max(first.y, second.y)) continue;
            const int x = first.x + static_cast<int>(
                static_cast<long long>(y - first.y) * (second.x - first.x) /
                (second.y - first.y));
            if (count < intersections.size()) intersections[count++] = x;
        }
        if (count < 2u) continue;
        for (std::size_t index = 1u; index < count; ++index) {
            const int value = intersections[index];
            std::size_t insertion = index;
            while (insertion != 0u && intersections[insertion - 1u] > value) {
                intersections[insertion] = intersections[insertion - 1u];
                --insertion;
            }
            intersections[insertion] = value;
        }
        for (std::size_t pair = 0; pair + 1u < count; pair += 2u) {
            const int start = std::max(0, intersections[pair]);
            const int end = std::min(kViewportRight, intersections[pair + 1u]);
            for (int x = start; x <= end; ++x) {
                const bool dark = darken_period != 0u &&
                    (static_cast<unsigned>(x + y) % darken_period) == 0u;
                pixel(framebuffer, x, y, dark ? 0u : color);
            }
        }
    }
}

int shape_height(unsigned shape) {
    if (shape >= 4u) return 18;
    if (shape != 0u) return 10;
    return 0;
}

void draw_isometric_cell(
    std::vector<std::uint8_t>& framebuffer,
    int center_x,
    int center_y,
    std::uint16_t cell,
    bool selected) {
    constexpr int half_width = 7;
    constexpr int half_depth = 4;
    const auto shape = static_cast<unsigned>((cell >> 8u) & 0x0fu);
    const int height = shape_height(shape);
    const auto color = editor_cell_color(cell);
    const auto edge = selected ? kSelectionColor :
        !editor_cell_has_geometry(cell) ? kGridColor : color;
    const Quad ground{{
        {center_x, center_y - half_depth},
        {center_x + half_width, center_y},
        {center_x, center_y + half_depth},
        {center_x - half_width, center_y},
    }};
    if (!editor_cell_has_geometry(cell)) {
        outline(framebuffer, ground, edge);
        return;
    }

    Quad top = ground;
    if (shape == 3u || shape == 5u) {
        top[0].y -= height;
        top[1].y -= height / 2;
        top[3].y -= height / 2;
    }
    else {
        for (auto& point : top) point.y -= height;
    }

    if (height != 0) {
        const Quad left_side{{top[3], top[2], ground[2], ground[3]}};
        const Quad right_side{{top[2], top[1], ground[1], ground[2]}};
        fill(framebuffer, left_side, color, 2u);
        fill(framebuffer, right_side, color, 4u);
        outline(framebuffer, left_side, edge);
        outline(framebuffer, right_side, edge);
    }
    fill(framebuffer, top, color);
    outline(framebuffer, top, edge);

    /* Arch cells get a dark opening on the visible near face. */
    if (shape == 1u && height >= 8) {
        const Point near = ground[2];
        const Quad opening{{
            {near.x - 3, near.y - 5}, {near.x + 3, near.y - 5},
            {near.x + 2, near.y - 1}, {near.x - 2, near.y - 1},
        }};
        fill(framebuffer, opening, 0u);
        outline(framebuffer, opening, edge);
    }
}

void render_isometric(
    std::vector<std::uint8_t>& framebuffer,
    const CustomLevel& level,
    std::size_t first_row,
    std::size_t selected_row,
    std::size_t selected_column,
    bool from_left) {
    struct Cell {
        std::size_t visible;
        std::size_t column;
    };
    std::vector<Cell> cells;
    for (std::size_t visible = 0; visible < kVisibleRows; ++visible) {
        if (first_row + visible >= level.row_count()) break;
        for (std::size_t column = 0; column < kCustomRoadColumns; ++column) {
            cells.push_back({visible, column});
        }
    }
    std::stable_sort(cells.begin(), cells.end(), [](const Cell& left, const Cell& right) {
        return left.visible + left.column < right.visible + right.column;
    });
    for (const auto cell : cells) {
        const auto row = first_row + cell.visible;
        const auto value = level.cells[row * kCustomRoadColumns + cell.column];
        const int center_x = from_left
            ? 132 + (static_cast<int>(cell.column) -
                static_cast<int>(cell.visible)) * 7
            : 68 + (static_cast<int>(cell.visible) -
                static_cast<int>(cell.column)) * 7;
        const int center_y = 42 + (static_cast<int>(cell.column) +
            static_cast<int>(cell.visible)) * 4;
        draw_isometric_cell(
            framebuffer, center_x, center_y, value,
            row == selected_row && cell.column == selected_column);
    }
}

void draw_straight_cell(
    std::vector<std::uint8_t>& framebuffer,
    const Quad& ground,
    std::uint16_t cell,
    int far_height,
    int near_height,
    bool selected) {
    const auto shape = static_cast<unsigned>((cell >> 8u) & 0x0fu);
    const auto color = editor_cell_color(cell);
    const auto edge = selected ? kSelectionColor :
        !editor_cell_has_geometry(cell) ? kGridColor : color;
    if (!editor_cell_has_geometry(cell)) {
        outline(framebuffer, ground, edge);
        return;
    }
    Quad top = ground;
    top[0].y -= far_height;
    top[1].y -= far_height;
    top[2].y -= near_height;
    top[3].y -= near_height;
    if (near_height != 0) {
        const Quad front{{top[3], top[2], ground[2], ground[3]}};
        fill(framebuffer, front, color, 2u);
        outline(framebuffer, front, edge);
    }
    fill(framebuffer, top, color);
    outline(framebuffer, top, edge);
    if (shape == 1u && near_height >= 6) {
        const int center = (ground[2].x + ground[3].x) / 2;
        const int bottom = (ground[2].y + ground[3].y) / 2;
        const int half = std::max(1, (ground[2].x - ground[3].x) / 5);
        const Quad opening{{
            {center - half, bottom - near_height / 2},
            {center + half, bottom - near_height / 2},
            {center + half, bottom - 1},
            {center - half, bottom - 1},
        }};
        fill(framebuffer, opening, 0u);
    }
}

void render_straight(
    std::vector<std::uint8_t>& framebuffer,
    const CustomLevel& level,
    std::size_t first_row,
    std::size_t selected_row,
    std::size_t selected_column) {
    for (std::size_t visible = 0; visible < kVisibleRows; ++visible) {
        const auto row = first_row + visible;
        if (row >= level.row_count()) break;
        const int far_y = 35 + static_cast<int>(visible) * 8;
        const int near_y = far_y + 8;
        const int far_half = 27 + static_cast<int>(visible) * 5;
        const int near_half = far_half + 5;
        for (std::size_t column = 0; column < kCustomRoadColumns; ++column) {
            const auto boundary = [column](int half_width, std::size_t edge) {
                return 101 - half_width + static_cast<int>(
                    (2 * half_width * static_cast<int>(column + edge)) /
                    static_cast<int>(kCustomRoadColumns));
            };
            const Quad ground{{
                {boundary(far_half, 0u), far_y},
                {boundary(far_half, 1u), far_y},
                {boundary(near_half, 1u), near_y},
                {boundary(near_half, 0u), near_y},
            }};
            const auto value = level.cells[row * kCustomRoadColumns + column];
            const auto shape = static_cast<unsigned>((value >> 8u) & 0x0fu);
            const int maximum_height = shape_height(shape);
            const int scaled_height = maximum_height == 0 ? 0 :
                std::max(3, maximum_height *
                    (7 + static_cast<int>(visible)) / 14);
            int far_height = scaled_height;
            int near_height = scaled_height;
            if (shape == 3u || shape == 5u) near_height = 0;
            draw_straight_cell(
                framebuffer, ground, value,
                far_height, near_height,
                row == selected_row && column == selected_column);
        }
    }
}

} // namespace

EditorViewMode next_editor_view(EditorViewMode mode) {
    const auto next = static_cast<unsigned>(mode) + 1u;
    return static_cast<EditorViewMode>(next % 4u);
}

std::string_view editor_view_name(EditorViewMode mode) {
    switch (mode) {
    case EditorViewMode::Top: return "TOP";
    case EditorViewMode::IsometricLeft: return "ISO LEFT";
    case EditorViewMode::Straight: return "STRAIGHT";
    case EditorViewMode::IsometricRight: return "ISO RIGHT";
    }
    return "TOP";
}

std::uint8_t editor_material_color(unsigned material) {
    switch (material & 0x0fu) {
    case 0u: return 0u;
    case 1u: return 2u;
    case 2u: return 56u;
    case 3u: return 30u;
    case 5u: return 64u;
    case 8u: return 190u;
    case 9u: return 96u;
    case 10u: return 70u;
    case 12u: return 55u;
    case 14u: return 143u;
    default: return 2u;
    }
}

bool editor_cell_has_geometry(std::uint16_t cell) {
    return (cell & 0x000fu) != 0u || (cell & 0x0f00u) != 0u;
}

std::uint8_t editor_cell_color(std::uint16_t cell) {
    const auto material = static_cast<unsigned>(cell & 0x000fu);
    const auto shape = static_cast<unsigned>((cell >> 8u) & 0x0fu);
    if (shape == 1u && material == 0u) {
        return 0x43u;
    }
    if (shape >= 2u && shape <= 5u) {
        const auto descriptor_color = static_cast<std::uint8_t>(
            (cell >> 4u) & 0x000fu);
        return descriptor_color == 0u ? 0x3du : descriptor_color;
    }
    return editor_material_color(material);
}

bool render_editor_spatial_view(
    std::vector<std::uint8_t>& framebuffer,
    const CustomLevel& level,
    std::size_t first_row,
    std::size_t selected_row,
    std::size_t selected_column,
    EditorViewMode mode) {
    if (framebuffer.size() != static_cast<std::size_t>(kScreenWidth * kScreenHeight) ||
        level.cells.size() != level.row_count() * kCustomRoadColumns ||
        first_row >= level.row_count() || selected_row >= level.row_count() ||
        selected_column >= kCustomRoadColumns || mode == EditorViewMode::Top) {
        return false;
    }
    if (mode == EditorViewMode::Straight) {
        render_straight(
            framebuffer, level, first_row, selected_row, selected_column);
    }
    else {
        render_isometric(
            framebuffer, level, first_row, selected_row, selected_column,
            mode == EditorViewMode::IsometricLeft);
    }
    return true;
}

} // namespace skyroads
