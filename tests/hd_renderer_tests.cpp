#include "hd_renderer.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

} // namespace

int main() {
    try {
        constexpr unsigned width = 5u;
        constexpr unsigned height = 4u;
        constexpr std::uint32_t black = 0x000000u;
        constexpr std::uint32_t road_color = 0xd42b55u;
        constexpr std::uint32_t sprite_color = 0xffffffu;
        std::vector<std::uint32_t> background(width * height, black);
        auto road = background;
        road[1u * width + 2u] = road_color;
        std::fill(road.begin() + 2u * width + 1u,
            road.begin() + 2u * width + 4u, road_color);
        std::fill(road.begin() + 3u * width,
            road.begin() + 4u * width, road_color);
        auto complete = road;
        complete[2u * width + 2u] = sprite_color;
        skyroads::RecoveredRoadShape shape;
        shape.palette_index = 1u;
        shape.color = road_color;
        shape.spans = {{1,2,3},{2,1,4},{3,0,5}};
        const std::vector<skyroads::RecoveredRoadShape> shapes{shape};
        skyroads::RecoveredShipModel no_ship;
        std::vector<std::uint8_t> no_exclusion(width * height);

        std::vector<std::uint32_t> output;
        skyroads::render_recovered_road_polygons(
            background, road, complete, shapes, shapes.size(), no_ship,
            no_exclusion, nullptr, nullptr, 1.0,
            width, height, 20u, 16u, output);
        require(output.size() == 20u * 16u,
            "Recovered-polygon renderer returned the wrong output size");
        require(output[6u * 20u + 7u] == road_color,
            "Individual TREK shape edges were not interpolated smoothly");
        require(output[9u * 20u + 9u] == sprite_color,
            "Recovered non-ship overlay was not restored above road geometry");
        require(std::all_of(output.begin(), output.end(), [](std::uint32_t color) {
                return color == black || color == road_color ||
                    color == sprite_color;
            }),
            "Recovered road rendering introduced filtered face colors");

        skyroads::render_recovered_road_polygons(
            background, road, complete, shapes, shapes.size(), no_ship,
            no_exclusion, nullptr, nullptr, 1.0,
            width, height, width, height, output);
        require(output == complete,
            "Recovered shape rasterizer changed a one-to-one frame");

        auto previous_shape = shape;
        for (auto& span : previous_shape.spans) {
            ++span.left;
            ++span.right;
        }
        const std::vector<skyroads::RecoveredRoadShape> previous_shapes{
            previous_shape};
        skyroads::render_recovered_road_polygons(
            background, road, complete, shapes, shapes.size(), no_ship,
            no_exclusion, &previous_shapes, nullptr, 0.0,
            width, height, width, height, output);
        require(output[1u * width + 3u] == road_color &&
                output[1u * width + 2u] == black,
            "Temporal TREK interpolation did not begin at prior geometry");

        constexpr unsigned ship_width = 40u;
        constexpr unsigned ship_height = 30u;
        std::vector<std::uint32_t> ship_background(
            ship_width * ship_height, black);
        auto ship_complete = ship_background;
        ship_complete[12u * ship_width + 20u] = sprite_color;
        std::vector<std::uint8_t> ship_exclusion(
            ship_width * ship_height);
        ship_exclusion[12u * ship_width + 20u] = 1u;
        skyroads::RecoveredShipModel ship;
        ship.visible = true;
        ship.shadow_visible = true;
        ship.center_x = 20.0;
        ship.center_y = 12.0;
        ship.shadow_y = 24.0;
        ship.outline = 0x080075u;
        ship.dark_blue = 0x103882u;
        ship.middle_blue = 0x4179aau;
        ship.light_blue = 0x69a2c3u;
        ship.highlight = 0x82bad3u;
        ship.engine_red = 0xd70800u;
        ship.engine_glow = 0xff5a20u;
        ship.shadow = 0x555561u;
        skyroads::render_recovered_road_polygons(
            ship_background, ship_background, ship_complete, {}, 0u, ship,
            ship_exclusion, nullptr, nullptr, 1.0,
            ship_width, ship_height, 160u, 120u, output);
        require(std::find(output.begin(), output.end(), ship.engine_red) !=
                output.end() &&
                std::find(output.begin(), output.end(), sprite_color) ==
                output.end(),
            "Hi-Def ship model did not replace the recovered sprite pixels");

        auto clipped_ship_mask = ship_exclusion;
        for (unsigned y = 0; y < ship_height; ++y) {
            for (unsigned x = ship_width / 2u; x < ship_width; ++x) {
                clipped_ship_mask[y * ship_width + x] |= 0x02u;
            }
        }
        skyroads::render_recovered_road_polygons(
            ship_background, ship_background, ship_complete, {}, 0u, ship,
            clipped_ship_mask, nullptr, nullptr, 1.0,
            ship_width, ship_height, 160u, 120u, output);
        bool left_engine = false;
        bool right_engine = false;
        for (unsigned y = 0; y < 120u; ++y) {
            for (unsigned x = 0; x < 160u; ++x) {
                if (output[y * 160u + x] != ship.engine_red) continue;
                if (x < 80u) left_engine = true;
                else right_engine = true;
            }
        }
        require(!left_engine && right_engine,
            "Recovered road visibility did not clip the Hi-Def ship model");

        bool rejected_invalid_frame = false;
        try {
            skyroads::render_recovered_road_polygons(
                {}, road, complete, shapes, shapes.size(), no_ship,
                no_exclusion, nullptr, nullptr, 1.0,
                width, height, 20u, 16u, output);
        }
        catch (const std::invalid_argument&) {
            rejected_invalid_frame = true;
        }
        require(rejected_invalid_frame,
            "Recovered-polygon renderer accepted an invalid source frame");
        std::cout << "Recovered TREK shape and ship renderer passed\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
