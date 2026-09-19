#include "recovered_game.hpp"
#include "custom_levels.hpp"
#include "editor_3d_view.hpp"
#include "embedded_game_data.hpp"
#include "expanded_level_menu.hpp"

extern "C" {
#include "gameplay.h"
#include "graphics_archive.h"
#include "road_archive.h"
}

#include <algorithm>
#include <array>
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
            skyroads::RecoveredGame quit_game(root);
            skyroads::NativeInput quit_input;
            quit_input.enter_pressed = true;
            quit_game.timer_tick(quit_input);
            quit_input = {};
            quit_input.escape_pressed = true;
            quit_game.timer_tick(quit_input);
            require(quit_game.quit_requested() &&
                    quit_game.screen() == skyroads::NativeScreen::MainMenu,
                "Main-menu Escape activated an item instead of requesting exit");
        }

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
                for (unsigned x = 24u; x < 82u; ++x) {
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
            require(editor_game.screen() == skyroads::NativeScreen::CustomLevelBrowser &&
                    editor_game.custom_level_count() == skyroads::kOriginalLevelCount,
                "The ORIGINAL LEVELS folder did not list all 30 recovered roads");
            input.down = true;
            editor_game.timer_tick(input);
            input = {};
            input.enter_pressed = true;
            editor_game.timer_tick(input);
            input = {};
            require(editor_game.screen() == skyroads::NativeScreen::CustomLevelEditor,
                "Original road 1-1 did not open in the built-in editor");
            const auto top_editor_hash = hash_bytes(editor_game.indexed_pixels());
            std::array<std::uint64_t, 3> spatial_hashes{};
            for (auto& spatial_hash : spatial_hashes) {
                input.editor_view_pressed = true;
                editor_game.timer_tick(input);
                input = {};
                spatial_hash = hash_bytes(editor_game.indexed_pixels());
            }
            require(spatial_hashes[0] != top_editor_hash &&
                    spatial_hashes[1] != top_editor_hash &&
                    spatial_hashes[2] != top_editor_hash &&
                    spatial_hashes[0] != spatial_hashes[1] &&
                    spatial_hashes[0] != spatial_hashes[2] &&
                    spatial_hashes[1] != spatial_hashes[2],
                "Editor did not expose distinct left, straight, and right 3D views");
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
            const auto options_root = root / "build" / "test-options-runtime";
            std::error_code error;
            std::filesystem::remove_all(options_root, error);
            std::filesystem::create_directories(options_root, error);
            require(!error, "Could not create the isolated Options test folder");
            std::filesystem::copy_file(
                root / "skyroads.exe", options_root / "skyroads.exe",
                std::filesystem::copy_options::overwrite_existing, error);
            require(!error,
                "Could not stage the original executable for Options testing");
            {
                std::ofstream config(options_root / "SKYROADS.NATIVE.CFG");
                config << "SKYROADS NATIVE 1\n"
                       << "high_definition=0\n"
                       << "display_aspect=0\n";
            }

            skyroads::RecoveredGame options_game(options_root);
            skyroads::NativeInput options_input;
            options_input.enter_pressed = true;
            options_game.timer_tick(options_input);
            options_input = {};
            for (unsigned item = 0; item < 4u; ++item) {
                options_input.down = true;
                options_game.timer_tick(options_input);
                options_input = {};
            }
            options_input.enter_pressed = true;
            options_game.timer_tick(options_input);
            options_input = {};
            require(options_game.screen() == skyroads::NativeScreen::Options,
                "OPTIONS did not open from the main menu");
            options_input.enter_pressed = true;
            options_game.timer_tick(options_input);
            options_input = {};
            require(options_game.high_definition_enabled(),
                "OPTIONS did not toggle Hi-Def polygons");
            options_input.down = true;
            options_game.timer_tick(options_input);
            options_input = {};
            options_input.right = true;
            options_game.timer_tick(options_input);
            options_input = {};
            require(options_game.aspect_ratio_mode() ==
                    skyroads::NativeAspectRatio::Widescreen,
                "OPTIONS did not select widescreen presentation");
            options_input.right = true;
            options_game.timer_tick(options_input);
            options_input = {};
            require(options_game.aspect_ratio_mode() ==
                    skyroads::NativeAspectRatio::UltraWidescreen,
                "OPTIONS did not select ultra-widescreen presentation");
            options_input.escape_pressed = true;
            options_game.timer_tick(options_input);
            require(options_game.screen() == skyroads::NativeScreen::MainMenu,
                "OPTIONS Escape did not return to the main menu");
            std::filesystem::remove_all(options_root, error);
        }

        {
            skyroads::RecoveredGame kosmonaut_game(root);
            input.enter_pressed = true;
            kosmonaut_game.timer_tick(input);
            input = {};
            for (unsigned item = 0; item < 5u; ++item) {
                input.down = true;
                kosmonaut_game.timer_tick(input);
                input = {};
            }
            std::size_t label_body = 0u;
            std::size_t label_halo = 0u;
            for (unsigned y = 182u; y < 200u; ++y) {
                for (unsigned x = 220u; x < 315u; ++x) {
                    const auto pixel = kosmonaut_game.indexed_pixels()[y * 320u + x];
                    if (pixel == 0xc0u) ++label_body;
                    if (pixel == 0xbfu) ++label_halo;
                }
            }
            require(label_body > 100u && label_halo > 100u,
                "Kosmonaut did not use the native main-menu label style");
            input.enter_pressed = true;
            kosmonaut_game.timer_tick(input);
            input = {};
            require(kosmonaut_game.screen() == skyroads::NativeScreen::Kosmonaut,
                "Kosmonaut did not open from the main menu");
            require(kosmonaut_game.indexed_pixels().size() == 320u * 200u,
                "Kosmonaut did not present its recovered EGA framebuffer");
            auto initial_kosmonaut_frame =
                hash_bytes(kosmonaut_game.indexed_pixels());
            for (unsigned irq = 0; irq < 10u; ++irq) {
                kosmonaut_game.timer_tick(input);
                const auto frame = hash_bytes(kosmonaut_game.indexed_pixels());
                if (frame != initial_kosmonaut_frame) {
                    initial_kosmonaut_frame = frame;
                    break;
                }
            }
            for (unsigned irq = 0; irq < 9u; ++irq) {
                kosmonaut_game.timer_tick(input);
                require(hash_bytes(kosmonaut_game.indexed_pixels()) ==
                        initial_kosmonaut_frame,
                    "Kosmonaut advanced at the too-fast SkyRoads cadence");
            }
            kosmonaut_game.timer_tick(input);
            require(hash_bytes(kosmonaut_game.indexed_pixels()) !=
                    initial_kosmonaut_frame,
                "Kosmonaut did not advance at its 386-paced timer step");
            input.escape_pressed = true;
            for (unsigned irq = 0; irq < 10u; ++irq) {
                kosmonaut_game.timer_tick(input);
                input = {};
            }
            require(kosmonaut_game.screen() == skyroads::NativeScreen::MainMenu,
                "Kosmonaut Escape did not return to the SkyRoads menu");
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
            const auto original_bytes = skyroads::embedded_game_file(
                skyroads::EmbeddedCampaign::SkyRoads, "roads.lzs");
            SrRoadArchive original_roads{};
            require(sr_load_road_archive(
                    original_bytes.data(), original_bytes.size(), &original_roads) ==
                    SR_ROAD_ARCHIVE_OK,
                "Could not decode the embedded original roads for import testing");
            require(original_roads.road_count == skyroads::kOriginalLevelCount + 1u,
                "Embedded original road archive has an unexpected record count");
            skyroads::CustomLevel final_original_level;
            for (std::size_t level_index = 0u;
                 level_index < skyroads::kOriginalLevelCount; ++level_index) {
                const auto world = level_index / 3u + 1u;
                const auto road = level_index % 3u + 1u;
                const auto name = std::to_string(world) + "-" +
                    std::to_string(road);
                skyroads::CustomLevel imported;
                require(skyroads::load_custom_level(
                        finish_root / "custom_levels" / "ORIGINAL LEVELS" /
                            (name + ".srlevel"),
                        imported),
                    "An original road was not materialized in ORIGINAL LEVELS");
                const auto& source = original_roads.roads[level_index + 1u];
                require(imported.name == name &&
                        imported.theme == level_index / 3u &&
                        imported.gravity == source.gravity &&
                        imported.fuel == source.fuel &&
                        imported.oxygen == source.oxygen &&
                        imported.cells.size() ==
                            source.row_count * skyroads::kCustomRoadColumns &&
                        std::equal(imported.cells.begin(), imported.cells.end(),
                            source.cells),
                    "An imported original road differs from its decoded ROADS.LZS record");
                if (level_index + 1u == skyroads::kOriginalLevelCount) {
                    final_original_level = imported;
                }
            }
            for (const auto view : {
                     skyroads::EditorViewMode::IsometricLeft,
                     skyroads::EditorViewMode::Straight,
                     skyroads::EditorViewMode::IsometricRight}) {
                std::vector<std::uint8_t> editor_frame(320u * 200u, 7u);
                require(skyroads::render_editor_spatial_view(
                        editor_frame, final_original_level,
                        45u, 45u, 0u, view),
                    "Could not render the original road 10-3 spatial fixture");
                require(std::count(
                        editor_frame.begin(), editor_frame.end(), 0x3du) != 0,
                    "Original road 10-3 lost its shape-only obstacle height");
            }
            sr_free_road_archive(&original_roads);
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
        require(game.high_definition_scene_available() &&
                game.high_definition_background_pixels().size() == 320u * 200u &&
                game.high_definition_road_pixels().size() == 320u * 200u &&
                !game.high_definition_road_shapes().empty() &&
                game.high_definition_ship_exclusion_mask().size() == 320u * 200u &&
                game.high_definition_ship_model().visible,
            "Recovered road geometry was not exposed for Hi-Def presentation");
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
        input = {};
        input.left = true;
        for (unsigned irq = 0; irq < 6u; ++irq) game.timer_tick(input);
        require(game.lateral_velocity() < 0,
            "Ctrl-F12 air steering did not turn left while airborne");
        input = {};
        input.right = true;
        for (unsigned irq = 0; irq < 6u; ++irq) game.timer_tick(input);
        require(game.lateral_velocity() > 0,
            "Ctrl-F12 air steering did not reverse direction while airborne");

        std::cout << "Native frontend is running the executable-authoritative core\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
