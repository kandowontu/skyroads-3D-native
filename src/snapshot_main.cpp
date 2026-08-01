#include "recovered_game.hpp"

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
        const auto view = argc > 3 ? std::string(argv[3]) : std::string("game");
        const bool editor_view = view == "editor" || view == "editor-iso-left" ||
            view == "editor-straight" || view == "editor-iso-right";
        const bool xmas_view = view == "xmas-levels" || view == "xmas-game" ||
            view == "creations" || editor_view || view == "custom-game";
        skyroads::RecoveredGame game(root);
        skyroads::NativeInput input;
        if (view != "intro" && view != "demo") {
            input.enter_pressed = true;
            game.timer_tick(input); // any key skips the recovered intro to the menu
            input = {};
        }
        if (view == "main-controls" || view == "main-editor") {
            const unsigned steps = view == "main-editor" ? 3u : 1u;
            for (unsigned step = 0; step < steps; ++step) {
                input.down = true;
                game.timer_tick(input);
                input = {};
            }
        }
        else if (view == "creations" || editor_view ||
            view == "custom-game") {
            for (unsigned item = 0; item < 3u; ++item) {
                input.down = true;
                game.timer_tick(input);
                input = {};
            }
            input.enter_pressed = true;
            game.timer_tick(input); // main menu -> custom road browser
            input = {};
            if (view != "creations") {
                input.down = true;
                game.timer_tick(input); // first saved creation
                input = {};
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
        else if (view == "settings" || view == "settings-hd" || view == "help") {
            input.down = true;
            game.timer_tick(input);
            if (view == "help") game.timer_tick(input);
            input = {};
            input.enter_pressed = true;
            game.timer_tick(input);
            input = {};
            for (int irq = 0; irq < 370; ++irq) game.timer_tick(input);
            if (view == "settings-hd") {
                input.down = true;
                game.timer_tick(input); // keyboard -> sound on
                input = {};
                input.down = true;
                game.timer_tick(input); // sound on -> hi-def
                input = {};
                input.enter_pressed = true;
                game.timer_tick(input); // enable hi-def
                input = {};
            }
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
        if (editor_view && view != "editor") {
            for (unsigned page = 0; page < 2u; ++page) {
                input.editor_page_down_pressed = true;
                game.timer_tick(input);
                input = {};
            }
            const unsigned views = view == "editor-iso-left" ? 1u :
                view == "editor-straight" ? 2u : 3u;
            for (unsigned index = 0; index < views; ++index) {
                input.editor_view_pressed = true;
                game.timer_tick(input);
                input = {};
            }
        }
        if (view == "game" || view == "xmas-game") {
            input.enter_pressed = true;
            game.timer_tick(input); // level selection -> road 1
            input = {};
            for (int irq = 0; irq < 370; ++irq) game.timer_tick(input);
            for (int irq = 0; irq < 475; ++irq) {
                input = {};
                input.up = true;
                input.right = irq >= 175 && irq < 260;
                input.jump = irq >= 330 && irq < 335;
                game.timer_tick(input);
            }
        }
        else if (view == "demo") {
            input = {};
            const int irqs = argc > 4 ? std::stoi(argv[4]) : 10000;
            for (int irq = 0; irq < irqs; ++irq) game.timer_tick(input);
        }

        std::ofstream stream(output, std::ios::binary);
        if (!stream) throw std::runtime_error("Unable to write " + output.string());
        stream << "P6\n" << skyroads::kScreenWidth << ' ' << skyroads::kScreenHeight << "\n255\n";
        for (const auto color : game.pixels()) {
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
