#include "recovered_game.hpp"
#include "expanded_level_menu.hpp"

extern "C" {
#include "graphics_archive.h"
}

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::uint64_t hash_bytes(const std::vector<std::uint8_t>& bytes) {
    std::uint64_t hash = UINT64_C(1469598103934665603);
    for (const auto value : bytes) {
        hash ^= value;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) throw std::runtime_error("Unable to read " + path.string());
    const auto size = stream.tellg();
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    stream.seekg(0);
    stream.read(reinterpret_cast<char*>(bytes.data()), size);
    return bytes;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const auto root = argc > 1
            ? std::filesystem::path(argv[1])
            : std::filesystem::current_path();
        skyroads::RecoveredGame game(root);
        require(game.level_count() ==
                (game.has_xmas_levels() ? skyroads::kCombinedLevelCount
                                        : skyroads::kOriginalLevelCount),
            "Native reconstruction exposed the wrong campaign level count");
        require(game.screen() == skyroads::NativeScreen::Intro,
            "Native reconstruction should begin in the recovered DOS intro");
        require(game.indexed_pixels().size() == 320u * 200u &&
                game.pixels().size() == 320u * 200u,
            "Native framebuffer dimensions are wrong");
        require(game.presentation_revision() != 0,
            "Initial recovered frame was not presented");

        {
            skyroads::RecoveredGame cadence_game(root);
            skyroads::NativeInput no_input;
            cadence_game.timer_tick(no_input);
            const auto presented = cadence_game.presentation_revision();
            cadence_game.timer_tick(no_input);
            require(cadence_game.presentation_revision() == presented,
                "Presentation revision advanced on a non-rendering 180 Hz IRQ");
        }

        skyroads::NativeInput input;
        input.enter_pressed = true;
        game.timer_tick(input);
        require(game.screen() == skyroads::NativeScreen::MainMenu,
            "Any key should take the recovered intro to the main menu");
        const auto main_hash = hash_bytes(game.indexed_pixels());

        {
            skyroads::RecoveredGame menu_game(root);
            menu_game.timer_tick(input);
            input = {};
            input.down = true;
            menu_game.timer_tick(input);
            input = {};
            require(menu_game.screen() == skyroads::NativeScreen::MainMenu,
                "Main-menu selection navigation left the menu");

            const auto bytes = read_file(root / "mainmenu.lzs");
            SrGraphicsArchive menu_items{};
            require(sr_load_vga_graphics_archive(
                    bytes.data(), bytes.size(), 0xbeu, &menu_items) ==
                    SR_GRAPHICS_ARCHIVE_OK && menu_items.picture_count == 3u,
                "Could not decode the original main-menu item states");
            const auto& expected = menu_items.pictures[1];
            std::size_t visible_background_pixels = 0u;
            for (std::size_t row = 0; row < expected.height; ++row) {
                for (std::size_t column = 0; column < expected.width; ++column) {
                    const auto source = expected.pixels[
                        row * expected.width + column];
                    const auto destination = menu_game.indexed_pixels()[
                        expected.screen_offset + row * 320u + column];
                    if (source != 0u) {
                        require(destination == source,
                            "Main-menu label pixels did not match the original state");
                    }
                    else if (destination != 0u) {
                        ++visible_background_pixels;
                    }
                }
            }
            require(visible_background_pixels != 0u,
                "Main-menu transparency did not preserve the title-road backdrop");
            sr_free_graphics_archive(&menu_items);
        }

        {
            skyroads::RecoveredGame editor_game(root);
            input.enter_pressed = true;
            editor_game.timer_tick(input);
            input = {};
            for (unsigned item = 0; item < 3u; ++item) {
                input.down = true;
                editor_game.timer_tick(input);
                input = {};
            }
            input.enter_pressed = true;
            editor_game.timer_tick(input);
            input = {};
            require(editor_game.screen() == skyroads::NativeScreen::CustomLevelBrowser &&
                    editor_game.custom_level_count() >= 2u,
                "The built-in creation browser did not list both demo roads");
            input.down = true;
            editor_game.timer_tick(input);
            input = {};
            input.enter_pressed = true;
            editor_game.timer_tick(input);
            input = {};
            require(editor_game.screen() == skyroads::NativeScreen::CustomLevelEditor,
                "A listed creation did not open in the built-in editor");
            input.escape_pressed = true;
            editor_game.timer_tick(input);
            input = {};
            input.editor_play_pressed = true;
            editor_game.timer_tick(input);
            input = {};
            require(editor_game.screen() == skyroads::NativeScreen::LevelTransition,
                "A custom creation did not enter the recovered play-test flow");
        }

        input.enter_pressed = true;
        game.timer_tick(input);
        input = {};
        for (unsigned irq = 0;
             irq < 400 && game.screen() != skyroads::NativeScreen::LevelSelection;
             ++irq) {
            game.timer_tick(input);
        }
        require(game.screen() == skyroads::NativeScreen::LevelSelection,
            "Main-menu Start did not enter the recovered level selector");
        for (unsigned irq = 0; irq < 200; ++irq) game.timer_tick(input);
        const auto level_menu_hash = hash_bytes(game.indexed_pixels());
        require(level_menu_hash != main_hash,
            "Level selection should not reuse the main-menu framebuffer");

        if (game.has_xmas_levels()) {
            input.right = true;
            game.timer_tick(input);
            input = {};
            game.timer_tick(input);
            input.right = true;
            game.timer_tick(input);
            input = {};
            require(game.level_index() == skyroads::kOriginalLevelCount,
                "Four-column selector did not reach the first SkyRoads Xmas level");
        }

        input.enter_pressed = true;
        game.timer_tick(input);
        input = {};
        for (unsigned irq = 0;
             irq < 220 && game.screen() == skyroads::NativeScreen::LevelSelection;
             ++irq) {
            game.timer_tick(input);
        }
        require(game.screen() == skyroads::NativeScreen::LevelTransition,
            "Level-selector Enter did not start the original run_level fade");
        for (unsigned irq = 0;
             irq < 220 && game.screen() == skyroads::NativeScreen::LevelTransition;
             ++irq) {
            game.timer_tick(input);
        }
        require(game.screen() == skyroads::NativeScreen::Playing,
            "Original 36-tick run_level fade did not enter gameplay");
        require(game.road_distance() == UINT32_C(0x00030000),
            "Recovered gameplay did not use the executable's initial distance");
        const auto initial_game_hash = hash_bytes(game.indexed_pixels());

        input.up = true;
        for (unsigned irq = 0; irq < 100; ++irq) game.timer_tick(input);
        require(game.road_distance() > UINT32_C(0x00030000),
            "Recovered fixed-point gameplay did not advance the ship");
        require(game.gameplay_ticks() >= 20,
            "180 Hz ISR schedule did not produce the original 36 Hz game ticks");
        require(hash_bytes(game.indexed_pixels()) != initial_game_hash,
            "Recovered renderer did not update the gameplay framebuffer");

        const auto fuel_before_refill = game.fuel();
        input = {};
        input.cheat_refill_pressed = true;
        game.timer_tick(input);
        require(game.fuel() > fuel_before_refill && game.oxygen() != 0u,
            "Ctrl-F11 resource refill did not update recovered gameplay state");

        input = {};
        input.cheat_no_gravity_pressed = true;
        game.timer_tick(input);
        require(game.no_gravity_enabled() && game.gravity_step() == 0,
            "Ctrl-F10 did not remove recovered gravity");
        input = {};
        input.cheat_no_gravity_pressed = true;
        game.timer_tick(input);
        require(!game.no_gravity_enabled() && game.gravity_step() != 0,
            "Ctrl-F10 did not restore recovered gravity when toggled off");

        input = {};
        input.cheat_overdrive_pressed = true;
        game.timer_tick(input);
        require(game.overdrive_enabled() && game.forward_speed_limit() == 0x5554,
            "Ctrl-F9 did not raise the recovered speed ceiling to 200 percent");

        input = {};
        input.cheat_air_jump_pressed = true;
        game.timer_tick(input);
        require(game.air_jump_enabled(),
            "Ctrl-F12 did not enable mid-air jumping");
        input = {};
        input.jump = true;
        for (unsigned irq = 0; irq < 10u; ++irq) game.timer_tick(input);
        input = {};
        for (unsigned irq = 0; irq < 10u; ++irq) game.timer_tick(input);
        const auto velocity_before_air_jump = game.vertical_velocity();
        input.jump = true;
        for (unsigned irq = 0; irq < 6u; ++irq) game.timer_tick(input);
        require(game.vertical_velocity() > velocity_before_air_jump,
            "Enabled Ctrl-F12 mode did not restart a jump while airborne");

        std::cout << "Native frontend is running the executable-authoritative core\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
