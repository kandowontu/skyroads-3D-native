#include "editor_3d_view.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::uint64_t hash(const std::vector<std::uint8_t>& bytes) {
    std::uint64_t value = UINT64_C(1469598103934665603);
    for (const auto byte : bytes) {
        value ^= byte;
        value *= UINT64_C(1099511628211);
    }
    return value;
}

int first_changed_row(const std::vector<std::uint8_t>& framebuffer) {
    for (int y = 31; y <= 181; ++y) {
        for (int x = 0; x <= 202; ++x) {
            if (framebuffer[static_cast<std::size_t>(y) * 320u + x] != 7u) {
                return y;
            }
        }
    }
    return 182;
}

std::size_t count_color(
    const std::vector<std::uint8_t>& framebuffer,
    std::uint8_t color) {
    return static_cast<std::size_t>(
        std::count(framebuffer.begin(), framebuffer.end(), color));
}

} // namespace

int main() {
    try {
        require(skyroads::next_editor_view(skyroads::EditorViewMode::Top) ==
                skyroads::EditorViewMode::IsometricLeft &&
                skyroads::next_editor_view(skyroads::EditorViewMode::IsometricLeft) ==
                skyroads::EditorViewMode::Straight &&
                skyroads::next_editor_view(skyroads::EditorViewMode::Straight) ==
                skyroads::EditorViewMode::IsometricRight &&
                skyroads::next_editor_view(skyroads::EditorViewMode::IsometricRight) ==
                skyroads::EditorViewMode::Top,
            "Editor camera did not cycle through all four views");
        require(skyroads::editor_view_name(skyroads::EditorViewMode::Straight) ==
                "STRAIGHT",
            "Straight editor camera has the wrong UI name");

        skyroads::CustomLevel level;
        level.cells.resize(16u * skyroads::kCustomRoadColumns);
        for (std::size_t row = 0; row < level.row_count(); ++row) {
            for (std::size_t column = 0; column < skyroads::kCustomRoadColumns;
                 ++column) {
                const auto shape = static_cast<std::uint16_t>((row + column) % 6u);
                level.cells[row * skyroads::kCustomRoadColumns + column] =
                    static_cast<std::uint16_t>((shape << 8u) | 1u);
            }
        }

        std::vector<std::uint8_t> left(320u * 200u, 7u);
        std::vector<std::uint8_t> straight(320u * 200u, 7u);
        std::vector<std::uint8_t> right(320u * 200u, 7u);
        require(skyroads::render_editor_spatial_view(
                left, level, 0u, 4u, 3u,
                skyroads::EditorViewMode::IsometricLeft),
            "Could not render the left isometric editor camera");
        require(skyroads::render_editor_spatial_view(
                straight, level, 0u, 4u, 3u,
                skyroads::EditorViewMode::Straight),
            "Could not render the straight editor camera");
        require(skyroads::render_editor_spatial_view(
                right, level, 0u, 4u, 3u,
                skyroads::EditorViewMode::IsometricRight),
            "Could not render the right isometric editor camera");
        require(hash(left) != hash(straight) && hash(left) != hash(right) &&
                hash(straight) != hash(right),
            "Editor camera projections produced identical pictures");

        std::size_t selection_pixels = 0u;
        for (const auto pixel : straight) {
            if (pixel == 1u) ++selection_pixels;
        }
        require(selection_pixels != 0u,
            "Selected editor block was not outlined in the 3D camera");

        skyroads::CustomLevel flat = level;
        skyroads::CustomLevel high = level;
        std::fill(flat.cells.begin(), flat.cells.end(), 0x0001u);
        std::fill(high.cells.begin(), high.cells.end(), 0x0401u);
        std::vector<std::uint8_t> flat_frame(320u * 200u, 7u);
        std::vector<std::uint8_t> high_frame(320u * 200u, 7u);
        require(skyroads::render_editor_spatial_view(
                    flat_frame, flat, 0u, 0u, 0u,
                    skyroads::EditorViewMode::IsometricLeft) &&
                skyroads::render_editor_spatial_view(
                    high_frame, high, 0u, 0u, 0u,
                    skyroads::EditorViewMode::IsometricLeft),
            "Could not render height-comparison fixtures");
        require(first_changed_row(high_frame) < first_changed_row(flat_frame),
            "High blocks did not visibly rise above flat road cells");

        require(skyroads::editor_cell_has_geometry(0x0200u) &&
                skyroads::editor_cell_has_geometry(0x0400u) &&
                !skyroads::editor_cell_has_geometry(0x0000u) &&
                skyroads::editor_cell_color(0x0200u) == 0x3du &&
                skyroads::editor_cell_color(0x0400u) == 0x3du,
            "Shape-only original descriptors were classified as empty cells");
        skyroads::CustomLevel shape_only = level;
        std::fill(shape_only.cells.begin(), shape_only.cells.end(), 0x0200u);
        std::vector<std::uint8_t> shape_only_iso(320u * 200u, 7u);
        std::vector<std::uint8_t> shape_only_straight(320u * 200u, 7u);
        require(skyroads::render_editor_spatial_view(
                    shape_only_iso, shape_only, 0u, 0u, 0u,
                    skyroads::EditorViewMode::IsometricLeft) &&
                skyroads::render_editor_spatial_view(
                    shape_only_straight, shape_only, 0u, 0u, 0u,
                    skyroads::EditorViewMode::Straight),
            "Could not render shape-only original road descriptors");
        require(count_color(shape_only_iso, 0x3du) != 0u &&
                count_color(shape_only_straight, 0x3du) != 0u,
            "Shape-only original descriptors did not produce visible height");

        std::cout << "Top, left isometric, straight, and right isometric editor views passed\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
