#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace skyroads {

struct RecoveredRoadSpan {
    std::int16_t y{};
    std::int16_t left{};
    std::int16_t right{};
};

struct RecoveredRoadShape {
    std::uint8_t palette_index{};
    std::uint32_t color{};
    std::int16_t road_row{};
    std::int16_t road_column{};
    std::uint16_t cell_descriptor{};
    std::uint16_t pointer_base{};
    std::uint16_t shape_offset{};
    std::uint8_t pointer_relative{};
    std::uint8_t shape_ordinal{};
    std::uint8_t direction{};
    std::vector<RecoveredRoadSpan> spans;
};

struct RoadVertex { double x{}, y{}, z{}; };
struct RoadFace {
    std::vector<RoadVertex> vertices;
    std::uint8_t palette_index{};
    std::uint32_t color{};
};
struct WideRoadScene {
    std::vector<RoadFace> faces;
    std::vector<std::uint8_t> cockpit_mask;
    std::vector<std::uint8_t> ship_indices;
    std::vector<std::uint32_t> ship_colors;
    int ship_x{}, ship_y{};
    double ship_depth{1.0 / 46.0};
};

struct RecoveredShipModel {
    bool visible{};
    bool shadow_visible{};
    bool visibility_mask_required{};
    double center_x{};
    double center_y{};
    double shadow_y{};
    double yaw{};
    double pitch{};
    double engine_pulse{};
    std::uint32_t outline{};
    std::uint32_t dark_blue{};
    std::uint32_t middle_blue{};
    std::uint32_t light_blue{};
    std::uint32_t highlight{};
    std::uint32_t engine_red{};
    std::uint32_t engine_glow{};
    std::uint32_t shadow{};
};

/*
 * Rerasterize the individual flat shapes reported by the recovered TREK
 * renderer in their original painter order. Optional previous geometry allows
 * the Win32 host to interpolate the original 36 Hz presentation smoothly
 * without changing simulation timing. In ship_exclusion_mask, bit 0 marks
 * pixels belonging to the recovered VGA ship/shadow and bit 1 marks source
 * pixels where the recovered road-visibility mask permits the Hi-Def ship.
 * When wide_scene is supplied, complete camera-space meshes and a depth buffer
 * replace the archived spans; the cockpit silhouette is composited last.
 */
void render_recovered_road_polygons(
    const std::vector<std::uint32_t>& background,
    const std::vector<std::uint32_t>& road_without_ship,
    const std::vector<std::uint32_t>& complete_frame,
    const std::vector<RecoveredRoadShape>& road_shapes,
    std::size_t ship_layer,
    const RecoveredShipModel& ship,
    const std::vector<std::uint8_t>& ship_exclusion_mask,
    const std::vector<RecoveredRoadShape>* previous_road_shapes,
    const RecoveredShipModel* previous_ship,
    double interpolation,
    unsigned source_width,
    unsigned source_height,
    unsigned destination_width,
    unsigned destination_height,
    std::vector<std::uint32_t>& destination,
    const WideRoadScene* wide_scene = nullptr);

} // namespace skyroads
