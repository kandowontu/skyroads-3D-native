#include "recovered_game.hpp"
#include "hd_renderer.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    try {
        const auto root = std::filesystem::absolute(
            argc > 1 ? std::filesystem::path(argv[1])
                     : std::filesystem::current_path());
        const auto output = argc > 2 ? std::filesystem::path(argv[2]) : std::filesystem::path("snapshot.ppm");
        const auto requested_view = argc > 3 ? std::string(argv[3]) : std::string("game");
        const bool xmas_game_view = requested_view.starts_with("xmas-game");
        const auto view = xmas_game_view ? requested_view.substr(5) : requested_view;
        const bool original_last_editor =
            view == "original-editor-10-3" ||
            view == "original-editor-10-3-iso-left" ||
            view == "original-editor-10-3-straight" ||
            view == "original-editor-10-3-iso-right";
        const bool original_levels_view = view == "original-levels" ||
            view == "original-editor" || original_last_editor;
        const bool editor_view = view == "editor" || view == "editor-iso-left" ||
            view == "editor-straight" || view == "editor-iso-right" ||
            view == "original-editor" || original_last_editor;
        const bool xmas_view = view == "xmas-levels" || xmas_game_view ||
            view == "creations" || editor_view || view == "custom-game" ||
            original_levels_view;
        skyroads::RecoveredGame game(root);
        skyroads::NativeInput input;
        if (view != "intro" && view != "demo" && view != "demo-hd") {
            input.enter_pressed = true;
            game.timer_tick(input); // any key skips the recovered intro to the menu
            input = {};
        }
        if (view == "main-controls" || view == "main-editor" ||
            view == "main-options" || view == "options" ||
            view == "options-wide" || view == "options-ultrawide" ||
            view == "settings-hd" ||
            view == "main-kosmonaut" || view == "kosmonaut-title" ||
            view == "kosmonaut-select" || view == "kosmonaut-game") {
            const unsigned steps = view == "main-editor" ? 3u :
                view == "main-controls" ? 1u :
                (view == "main-options" || view == "options" ||
                 view == "options-wide" || view == "options-ultrawide" ||
                 view == "settings-hd") ? 4u : 5u;
            for (unsigned step = 0; step < steps; ++step) {
                input.down = true;
                game.timer_tick(input);
                input = {};
            }
            if (view == "options" || view == "options-wide" ||
                view == "options-ultrawide" || view == "settings-hd") {
                input.enter_pressed = true;
                game.timer_tick(input);
                input = {};
                if (view == "settings-hd") {
                    input.enter_pressed = true;
                    game.timer_tick(input);
                    input = {};
                }
                if (view == "options-wide" || view == "options-ultrawide") {
                    input.down = true;
                    game.timer_tick(input);
                    input = {};
                    input.right = true;
                    game.timer_tick(input);
                    input = {};
                    if (view == "options-ultrawide") {
                        input.right = true;
                        game.timer_tick(input);
                        input = {};
                    }
                }
            }
            else if (view == "kosmonaut-title" || view == "kosmonaut-select" ||
                view == "kosmonaut-game") {
                const auto press_kosmonaut_enter = [&]() {
                    input.enter_pressed = true;
                    for (int irq = 0; irq < 10; ++irq) {
                        game.timer_tick(input);
                        input = {};
                    }
                };
                press_kosmonaut_enter(); // combined menu -> Kosmonaut title
                if (view == "kosmonaut-select" || view == "kosmonaut-game") {
                    press_kosmonaut_enter(); // title -> original tutorial
                    press_kosmonaut_enter(); // tutorial -> high-score road selector
                }
                if (view == "kosmonaut-game") {
                    press_kosmonaut_enter(); // selector -> road
                    for (int irq = 0; irq < 240; ++irq) {
                        input.up = true;
                        input.right = irq > 110 && irq < 145;
                        input.jump = irq > 175 && irq < 185;
                        game.timer_tick(input);
                    }
                }
            }
        }
        else if (view == "creations" || editor_view ||
            view == "custom-game" || original_levels_view) {
            for (unsigned item = 0; item < 3u; ++item) {
                input.down = true;
                game.timer_tick(input);
                input = {};
            }
            input.enter_pressed = true;
            game.timer_tick(input); // main menu -> custom road browser
            input = {};
            if (original_levels_view) {
                input.down = true;
                game.timer_tick(input); // ORIGINAL LEVELS folder
                input = {};
                input.enter_pressed = true;
                game.timer_tick(input);
                input = {};
                if (view == "original-editor" || original_last_editor) {
                    const unsigned road_items = original_last_editor ? 30u : 1u;
                    for (unsigned item = 0; item < road_items; ++item) {
                        input.down = true;
                        game.timer_tick(input); // road 1-1 or road 10-3
                        input = {};
                    }
                    input.enter_pressed = true;
                    game.timer_tick(input);
                    input = {};
                }
            }
            else if (view != "creations") {
                for (unsigned item = 0; item < 2u; ++item) {
                    input.down = true;
                    game.timer_tick(input); // skip folder, then select first creation
                    input = {};
                }
                if (editor_view) input.enter_pressed = true;
                else input.editor_play_pressed = true;
                game.timer_tick(input);
                input = {};
                if (view == "custom-game") {
                    for (int irq = 0; irq < 370; ++irq) game.timer_tick(input);
                    for (int irq = 0; irq < 300; ++irq) {
                        input.up = true;
                        game.timer_tick(input);
                    }
                    input = {};
                }
            }
        }
        else if (view == "settings" || view == "help") {
            input.down = true;
            game.timer_tick(input);
            if (view == "help") game.timer_tick(input);
            input = {};
            input.enter_pressed = true;
            game.timer_tick(input);
            input = {};
            for (int irq = 0; irq < 370; ++irq) game.timer_tick(input);
        }
        else if (view != "main" && view != "intro" && view != "demo") {
            input.enter_pressed = true;
            game.timer_tick(input); // main menu -> level selection
            input = {};
            for (int irq = 0; irq < 370; ++irq) game.timer_tick(input);
        }
        if (xmas_view) {
            input.right = true;
            game.timer_tick(input);
            input = {};
            game.timer_tick(input);
            input.right = true;
            game.timer_tick(input);
            input = {};
        }
        if (editor_view && view != "editor" && view != "original-editor" &&
            view != "original-editor-10-3") {
            const unsigned pages = original_last_editor ? 3u : 2u;
            for (unsigned page = 0; page < pages; ++page) {
                input.editor_page_down_pressed = true;
                game.timer_tick(input);
                input = {};
            }
            const unsigned views =
                view == "editor-iso-left" ||
                    view == "original-editor-10-3-iso-left" ? 1u :
                view == "editor-straight" ||
                    view == "original-editor-10-3-straight" ? 2u : 3u;
            for (unsigned index = 0; index < views; ++index) {
                input.editor_view_pressed = true;
                game.timer_tick(input);
                input = {};
            }
        }
        if (view == "game" || view == "game-wide" ||
            view == "game-ultrawide" || view == "game-hd" ||
            view == "game-hd-wide" || view == "game-hd-ultrawide" ||
            view == "xmas-game") {
            input.enter_pressed = true;
            game.timer_tick(input); // level selection -> road 1
            input = {};
            for (int irq = 0; irq < 370; ++irq) game.timer_tick(input);
            const int drive_irqs = argc > 4 ? std::stoi(argv[4]) : 475;
            for (int irq = 0; irq < drive_irqs; ++irq) {
                input = {};
                input.up = true;
                input.right = irq >= 175 && irq < 260;
                input.jump = irq >= 330 && irq < 335;
                game.timer_tick(input);
            }
        }
        else if (view == "demo" || view == "demo-hd") {
            input = {};
            const int irqs = argc > 4 ? std::stoi(argv[4]) : 10000;
            for (int irq = 0; irq < irqs; ++irq) game.timer_tick(input);
        }



        unsigned output_width = skyroads::kScreenWidth;
        unsigned output_height = skyroads::kScreenHeight;
        const std::vector<std::uint32_t>* output_pixels = &game.pixels();
        std::vector<std::uint32_t> hd_pixels;
        if ((view == "game-wide" || view == "game-ultrawide" ||
             view == "game-hd" || view == "game-hd-wide" ||
             view == "game-hd-ultrawide" || view == "demo-hd") &&
            game.high_definition_scene_available()) {
            const bool classic_wide = view == "game-wide" ||
                view == "game-ultrawide";
            output_width = view == "game-wide" ? 356u :
                view == "game-ultrawide" ? 467u :
                view == "game-hd-wide" ? 1067u :
                view == "game-hd-ultrawide" ? 1400u : 960u;
            output_height = classic_wide ? 200u : 600u;
            const skyroads::RecoveredShipModel classic_ship{};
            std::vector<std::uint8_t> classic_ship_mask(
                static_cast<std::size_t>(skyroads::kScreenWidth) *
                skyroads::kScreenHeight, 0u);
            skyroads::render_recovered_road_polygons(
                game.high_definition_background_pixels(),
                game.high_definition_road_pixels(), game.pixels(),
                game.high_definition_road_shapes(),
                game.high_definition_ship_layer(),
                classic_wide ? classic_ship : game.high_definition_ship_model(),
                classic_wide ? classic_ship_mask
                             : game.high_definition_ship_exclusion_mask(),
                nullptr, nullptr, 1.0,
                skyroads::kScreenWidth, skyroads::kScreenHeight,
                output_width, output_height, hd_pixels,
                (view == "game-wide" || view == "game-ultrawide" ||
                 view == "game-hd-wide" || view == "game-hd-ultrawide")
                    ? &game.wide_road_scene() : nullptr);
            output_pixels = &hd_pixels;
        }

        std::ofstream stream(output, std::ios::binary);
        if (!stream) throw std::runtime_error("Unable to write " + output.string());
        stream << "P6\n" << output_width << ' ' << output_height << "\n255\n";
        for (const auto color : *output_pixels) {
            stream.put(static_cast<char>((color >> 16U) & 0xffU));
            stream.put(static_cast<char>((color >> 8U) & 0xffU));
            stream.put(static_cast<char>(color & 0xffU));
        }
        std::cout << "Wrote " << output << " at fixed-point road distance "
                  << game.road_distance() << " after " << game.gameplay_ticks()
                  << " original game ticks\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
