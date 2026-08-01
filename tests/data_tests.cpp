#include "recovered_game.hpp"
#include "custom_levels.hpp"
#include "expanded_level_menu.hpp"

extern "C" {
#include "gameplay.h"
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
            std::size_t editor_white_pixels = 0u;
            std::size_t editor_halo_pixels = 0u;
            for (unsigned y = 182u; y < 200u; ++y) {
                for (unsigned x = 134u; x < 187u; ++x) {
                    const auto pixel = editor_game.indexed_pixels()[y * 320u + x];
                    if (pixel == 0xc0u) ++editor_white_pixels;
                    if (pixel == 0xbfu) ++editor_halo_pixels;
                }
            }
            require(editor_white_pixels > 50u && editor_halo_pixels > 50u,
                "Editor did not use the original menu's white-body/yellow-halo style");
            for (unsigned y = 128u; y < 147u; ++y) {
                for (unsigned x = 127u; x < 195u; ++x) {
                    require(editor_game.indexed_pixels()[y * 320u + x] != 0xa1u,
                        "Intro's default Start highlight leaked into the Editor state");
                }
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

        {
            const auto finish_root = root / "build" / "test-finish-runtime";
            std::error_code error;
            std::filesystem::remove_all(finish_root, error);
            std::filesystem::create_directories(
                finish_root / "custom_levels", error);
            require(!error, "Could not create the isolated finish-coast test folder");
            std::filesystem::copy_file(
                root / "skyroads.exe", finish_root / "skyroads.exe",
                std::filesystem::copy_options::overwrite_existing, error);
            require(!error, "Could not stage the original executable for finish testing");

            skyroads::CustomLevel finish_level;
            finish_level.path =
                finish_root / "custom_levels" / "000_finish_tube.srlevel";
            finish_level.name = "FINISH TUBE TEST";
            finish_level.gravity = 8u;
            finish_level.cells.assign(24u * skyroads::kCustomRoadColumns, 0x0001u);
            for (std::size_t row = 22u; row < 24u; ++row) {
                for (std::size_t column = 0u;
                     column < skyroads::kCustomRoadColumns; ++column) {
                    finish_level.cells[row * skyroads::kCustomRoadColumns + column] =
                        0x010eu;
                }
            }
            require(skyroads::save_custom_level(finish_level),
                "Could not save the isolated finish-coast test road");

            skyroads::RecoveredGame finish_game(finish_root);
            skyroads::NativeInput finish_input;
            finish_input.enter_pressed = true;
            finish_game.timer_tick(finish_input);
            finish_input = {};
            for (unsigned item = 0; item < 3u; ++item) {
                finish_input.down = true;
                finish_game.timer_tick(finish_input);
                finish_input = {};
            }
            finish_input.enter_pressed = true;
            finish_game.timer_tick(finish_input);
            finish_input = {};
            finish_input.down = true;
            finish_game.timer_tick(finish_input);
            finish_input = {};
            finish_input.editor_play_pressed = true;
            finish_game.timer_tick(finish_input);
            finish_input = {};
            for (unsigned irq = 0;
                 irq < 1000u &&
                 finish_game.screen() == skyroads::NativeScreen::LevelTransition;
                 ++irq) {
                finish_game.timer_tick(finish_input);
            }
            require(finish_game.screen() == skyroads::NativeScreen::Playing,
                "Finish-coast test road did not start");

            bool saw_finish_counter_reset = false;
            auto previous_ticks = finish_game.gameplay_ticks();
            finish_input.up = true;
            for (unsigned irq = 0;
                 irq < 20000u &&
                 finish_game.screen() == skyroads::NativeScreen::Playing;
                 ++irq) {
                finish_game.timer_tick(finish_input);
                const auto current_ticks = finish_game.gameplay_ticks();
                if (!saw_finish_counter_reset && current_ticks < previous_ticks) {
                    saw_finish_counter_reset = true;
                    require(current_ticks == 0u,
                        "Finish-tube routine did not reset the original tick counter");
                }
                if (saw_finish_counter_reset &&
                    finish_game.screen() == skyroads::NativeScreen::Playing) {
                    require(current_ticks < SR_GAMEPLAY_FINISH_TICKS,
                        "Finish-tube coast remained active past 72 ticks");
                }
                previous_ticks = current_ticks;
            }
            require(saw_finish_counter_reset,
                "Test road did not enter the recovered finish-tube routine");
            require(finish_game.screen() == skyroads::NativeScreen::LevelResult &&
                    finish_game.gameplay_ticks() == SR_GAMEPLAY_FINISH_TICKS,
                "Road completion did not wait for the exact 72-tick tube coast");
            std::filesystem::remove_all(finish_root, error);
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
