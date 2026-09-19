#include "hd_renderer.hpp"
#include "wide_road_mesh.hpp"
#include "embedded_game_data.hpp"
extern "C" {
#include "renderer_vga.h"
}

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

} // namespace

int main(int argc, char** argv) {
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
            width, height, 24u, 16u, output);
        require(output[14u * 24u] == black &&
                output[14u * 24u + 2u] == road_color &&
                output[14u * 24u + 21u] == road_color &&
                output[14u * 24u + 23u] == black,
            "A viewport-edge polygon was nonsensically stretched through the "
            "widescreen area");
        auto offscreen_shape = shape;
        offscreen_shape.spans.back().left = -1;
        offscreen_shape.spans.back().right = 6;
        skyroads::render_recovered_road_polygons(
            background, road, complete, {offscreen_shape}, 1u, no_ship,
            no_exclusion, nullptr, nullptr, 1.0,
            width, height, 24u, 16u, output);
        require(output[14u * 24u] == road_color &&
                output[14u * 24u + 23u] == road_color,
            "True off-screen TREK coordinates were not projected into the "
            "widescreen area");

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

        ship.visibility_mask_required = true;
        skyroads::render_recovered_road_polygons(
            ship_background, ship_background, ship_complete, {}, 0u, ship,
            ship_exclusion, nullptr, nullptr, 1.0,
            ship_width, ship_height, 160u, 120u, output);
        require(std::find(output.begin(), output.end(), ship.engine_red) ==
                output.end(),
            "A fully road-occluded falling ship was drawn over the road");

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

        // Real geometry must retain the center view when the horizontal field
        // of view grows, and survive a near-plane crossing without mirroring.
        {
            constexpr unsigned sw=320, sh=200, ww=468;
            std::vector<std::uint32_t> blank(sw*sh,black);
            std::vector<std::uint8_t> mask(sw*sh,0);
            skyroads::WideRoadScene mesh;
            mesh.faces.push_back({{{-5,.2,.02},{5,.2,.02},
                {5,2,.02},{-5,2,.02}},1,road_color});
            const auto render=[&](unsigned w) {
                skyroads::render_recovered_road_polygons(blank,blank,blank,{},0,
                    no_ship,mask,nullptr,nullptr,1,sw,sh,w,sh,output,&mesh);
            };
            render(sw);
            const auto center=output;
            render(ww);
            for(unsigned y=0;y<sh;++y) for(unsigned x=0;x<sw;++x)
                require(output[y*ww+x+74]==center[y*sw+x],
                    "Widening the frustum changed its central projection");
            require(output[60*ww+10]==road_color && output[60*ww+ww-11]==road_color,
                "Complete mesh still stops at the original screen aperture");
            mesh.faces={{{{-1,0,.02},{1,0,.02},{1,1,-.02},{-1,1,-.02}},1,road_color}};
            render(ww);
            require(std::count(output.begin(),output.end(),road_color)>1000,
                "A face crossing the camera was dropped instead of clipped");
            for(auto& v:mesh.faces[0].vertices) v.z=-.02;
            render(ww);
            require(std::count(output.begin(),output.end(),road_color)==0,
                "Geometry behind the camera reflected into the viewport");
            mesh.ship_indices.assign(29*24,1);
            mesh.ship_colors.assign(29*24,sprite_color);
            mesh.ship_x=-20;
            mesh.ship_y=60;
            render(ww);
            require(output[65*ww+60]==sprite_color,
                "The classic ship is still clipped at the old left edge");
            mesh.faces={{{{-5,0,.01},{5,0,.01},{5,2,.01},{-5,2,.01}},1,road_color}};
            render(ww);
            require(output[65*ww+60]==road_color,
                "A ship behind road geometry was composited over it");
            mesh.ship_indices.clear();
            mesh.ship_colors.clear();

            const auto bytes=skyroads::embedded_game_file(
                skyroads::EmbeddedCampaign::SkyRoads,"trekdat.lzs");
            SrTrekArchive trek{};
            require(sr_load_trek_archive(bytes.data(),bytes.size(),&trek)==SR_TREK_ARCHIVE_OK,
                "Unable to load perspective calibration");
            std::vector<std::uint16_t> cells(24*7,0);
            for(unsigned type=0;type<6;++type) {
                std::fill(cells.begin(),cells.end(),0);
                for(unsigned row=0;row<24;++row) cells[row*7+3]=1;
                cells[8*7]=cells[8*7+6]=static_cast<std::uint16_t>(5|(type<<8));
                cells[10*7+3]=static_cast<std::uint16_t>(5|(type<<8));
                for(unsigned step=0;step<24;++step) {
                    build_wide_road_mesh(mesh,trek,cells.data(),24,
                        8*65536+step*4096);
                    require(!mesh.faces.empty(),"Mesh lost a populated road");
                    for(auto& f:mesh.faces) {
                        f.color = f.palette_index==1 ? 0x8e8d9fu :
                            f.palette_index==5 ? 0x86755eu :
                            f.palette_index==61 ? 0xff3672u :
                            f.palette_index==62 ? 0xd12951u :
                            f.palette_index==63 ? 0xb32a50u :
                            f.palette_index==64 ? 0x91273fu : 0x60b0d0u;
                        for(auto v:f.vertices) require(std::isfinite(v.x)&&
                            std::isfinite(v.y)&&std::isfinite(v.z),
                            "A road shape produced nonfinite camera coordinates");
                    }
                    render(ww);
                    if(argc>1 && step==6) {
                        std::ofstream image(std::string(argv[1])+"-type"+
                            std::to_string(type)+".ppm",std::ios::binary);
                        image<<"P6\n"<<ww<<' '<<sh<<"\n255\n";
                        for(auto pixel:output) {
                            image.put(static_cast<char>(pixel>>16));
                            image.put(static_cast<char>(pixel>>8));
                            image.put(static_cast<char>(pixel));
                        }
                        std::vector<std::uint8_t> vga(sw*sh),sky(sw*sh);
                        SrVgaRendererState state{};
                        sr_vga_renderer_state_init(&state);
                        SrRoadFrameParams params{};
                        params.road_phase=(8*65536+step*4096)/8192;
                        params.ship_frame=0xffff;
                        require(sr_draw_road_scene_vga(&trek,cells.data(),24,
                            &params,nullptr,nullptr,&state,sky.data(),vga.data())!=0,
                            "Could not render original geometry fixture");
                        std::ofstream original(std::string(argv[1])+"-original-type"+
                            std::to_string(type)+".ppm",std::ios::binary);
                        original<<"P6\n320 200\n255\n";
                        for(auto index:vga) {
                            std::uint32_t pixel=0;
                            for(const auto& f:mesh.faces) if(f.palette_index==index) pixel=f.color;
                            original.put(static_cast<char>(pixel>>16));
                            original.put(static_cast<char>(pixel>>8));
                            original.put(static_cast<char>(pixel));
                        }
                    }
                }
            }
            sr_free_trek_archive(&trek);
        }

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
