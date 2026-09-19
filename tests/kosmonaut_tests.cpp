#include "kosmonaut_assets.hpp"
#include "kosmonaut_game.hpp"
#include "recovered_game.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::uint64_t hash(std::span<const std::uint8_t> bytes) {
    std::uint64_t value = UINT64_C(14695981039346656037);
    for (const auto byte : bytes) {
        value ^= byte;
        value *= UINT64_C(1099511628211);
    }
    return value;
}

void write_frame(const char* path, const skyroads::KosmonautGame& game) {
    std::ofstream image(path, std::ios::binary);
    image << "P6\n320 200\n255\n";
    for (const auto pixel : game.pixels()) {
        const auto offset = static_cast<std::size_t>(pixel) * 3u;
        for (unsigned component = 0; component < 3u; ++component) {
            const auto six_bit = game.palette()[offset + component];
            image.put(static_cast<char>(
                static_cast<unsigned>(six_bit) * 255u / 63u));
        }
    }
    std::ofstream indexed(std::string(path) + ".idx", std::ios::binary);
    indexed.write(reinterpret_cast<const char*>(game.pixels().data()),
        static_cast<std::streamsize>(game.pixels().size()));
}

} // namespace

int main(int argc, char** argv) {
    try {
        require(skyroads::kosmonaut_screens().size() == 5u * 320u * 200u,
            "The five recovered Kosmonaut EGA screens are incomplete");
        require(skyroads::kosmonaut_roads().size() == 26u * 200u * 6u,
            "The 26 recovered Kosmonaut road maps are incomplete");
        require(skyroads::kosmonaut_demo_road().size() == 200u * 6u,
            "The separate Kosmonaut demonstration road is incomplete");
        require(skyroads::kosmonaut_tutorial_road().size() == 200u * 6u,
            "The separate Kosmonaut tutorial road is incomplete");
        require(skyroads::kosmonaut_font().size() == 87u * 8u,
            "The recovered Kosmonaut font is incomplete");
        require(skyroads::kosmonaut_font_styles().size() == 5u * 8u,
            "The recovered Kosmonaut font color styles are incomplete");
        require(skyroads::kosmonaut_music().size() == 0x118cu,
            "The recovered Kosmonaut PC-speaker note streams are incomplete");
        require(skyroads::kosmonaut_star_motion().size() == 0x10u &&
                skyroads::kosmonaut_star_rng().size() == 0x20u &&
                skyroads::kosmonaut_star_path_pointers().size() == 110u &&
                skyroads::kosmonaut_star_paths().size() == 0x08eeu,
            "The recovered Kosmonaut starfield records are incomplete");
        require(skyroads::kosmonaut_level_effects().size() == 26u * 0x32u,
            "The recovered animated-road records are incomplete");
        require(skyroads::kosmonaut_demo_input().size() == 0x2f1u,
            "The position-indexed Kosmonaut demo input is incomplete");
        require(skyroads::kosmonaut_palette_registers().size() == 16u,
            "The recovered EGA palette map is incomplete");
        require(skyroads::kosmonaut_ship_sprites().size() == 98u * 26u * 9u,
            "The original Kosmonaut ship frames are incomplete");
        require(skyroads::kosmonaut_render_masks().size() == 512u &&
                skyroads::kosmonaut_render_pointers().size() == 792u &&
                skyroads::kosmonaut_render_shapes().size() == 31035u,
            "The original Kosmonaut renderer records are incomplete");
        require(skyroads::kosmonaut_simulation_tables().size() == 0xb0u &&
                skyroads::kosmonaut_destruction_frames().size() == 68u,
            "The original Kosmonaut simulation lookup records are incomplete");
        require(skyroads::kosmonaut_title_scroll().size() == 29u &&
                skyroads::kosmonaut_tutorial_scroll().size() == 124u,
            "The original Kosmonaut footer crawls are incomplete");
        require(hash(skyroads::kosmonaut_screens()) == UINT64_C(0x22caa4930b86ce5b),
            "The recovered Kosmonaut EGA screens changed");
        require(hash(skyroads::kosmonaut_roads()) == UINT64_C(0x6ba3db52546d9996),
            "The one-based set of 26 playable Kosmonaut roads changed");
        require(hash(skyroads::kosmonaut_demo_road()) == UINT64_C(0xe1263814b78066f1),
            "The recovered Kosmonaut demonstration road changed");
        require(hash(skyroads::kosmonaut_tutorial_road()) ==
                UINT64_C(0xd24cc8ad1ebcd46f),
            "The recovered Kosmonaut tutorial road changed");
        require(hash(skyroads::kosmonaut_ship_sprites()) ==
                UINT64_C(0x2dc78b3bc33eb711),
            "The recovered Kosmonaut ship frames changed");
        require(hash(skyroads::kosmonaut_render_shapes()) ==
                UINT64_C(0x501f5d034d9151d6),
            "The recovered Kosmonaut span records changed");
        require(hash(skyroads::kosmonaut_level_effects()) ==
                UINT64_C(0x5a62847539929d7a),
            "The one-based animated-road records changed");

        const auto isolated = std::filesystem::temp_directory_path() /
            "skyroads-native-kosmonaut-test";
        std::error_code error;
        std::filesystem::create_directories(isolated, error);
        require(!error, "Could not create the isolated Kosmonaut test folder");
        /* The isolated folder intentionally contains no DOS executable. */
        skyroads::KosmonautGame game(isolated);
        game.enter();
        (void)game.consume_sound();
        require(game.pixels().size() == 320u * 200u,
            "Kosmonaut did not render a 320x200 native frame");
        const auto screens = skyroads::kosmonaut_screens();
        const auto different_pixels = [](std::span<const std::uint8_t> left,
                                          std::span<const std::uint8_t> right,
                                          unsigned x0, unsigned y0,
                                          unsigned x1, unsigned y1) {
            std::size_t result = 0u;
            for (unsigned y = y0; y < y1; ++y) {
                for (unsigned x = x0; x < x1; ++x) {
                    const auto index = static_cast<std::size_t>(y) * 320u + x;
                    if (left[index] != right[index]) ++result;
                }
            }
            return result;
        };
        require(different_pixels(game.pixels(),
                    screens.subspan(3u * 320u * 200u, 320u * 200u),
                    0u, 105u, 320u, 185u) > 300u,
            "Kosmonaut title credits and menu flow disappeared");
        require(game.unlocked_roads() >= 2u,
            "Kosmonaut did not preserve the original initial road access");

        skyroads::NativeInput input;
        input.enter_pressed = true;
        require(!game.tick(input), "Kosmonaut unexpectedly exited on title input");
        const auto held_title = game.pixels();
        require(!game.consume_sound(),
            "Kosmonaut title music changed before the original exit delay");
        input = {};
        for (unsigned tick = 0; tick < 5u; ++tick) {
            game.tick(input);
            require(game.pixels() == held_title,
                "Kosmonaut title text cleared before its original delay");
        }
        game.tick(input);
        require(game.pixels() != held_title,
            "Kosmonaut title text did not clear when the tutorial appeared");
        const auto tutorial_music = game.consume_sound();
        require(tutorial_music && tutorial_music->loop &&
                tutorial_music->effect == 0x20001u &&
                tutorial_music->samples.size() > 250000u,
            "Kosmonaut tutorial did not start its recovered PC-speaker score");
        const auto tutorial_range = std::minmax_element(
            tutorial_music->samples.begin(), tutorial_music->samples.end());
        require(*tutorial_range.first < 100u && *tutorial_range.second > 150u,
            "Kosmonaut tutorial score did not synthesize its PIT waveform");
        input.enter_pressed = true;
        require(!game.tick(input) && !game.playing(),
            "Kosmonaut did not enter its original road selector");
        const auto selector_music = game.consume_sound();
        require(selector_music && selector_music->loop &&
                selector_music->effect == 0x20002u &&
                selector_music->samples.size() > 200000u,
            "Kosmonaut selector did not start its recovered PC-speaker score");
        require(different_pixels(game.pixels(),
                    screens.subspan(0u, 320u * 200u),
                    0u, 0u, 320u, 135u) > 300u,
            "Kosmonaut high-score road-selection menu disappeared");
        input = {};
        input.enter_pressed = true;
        require(!game.tick(input) && game.playing(),
            "Kosmonaut did not start its native road simulation");
        const auto sprites = skyroads::kosmonaut_ship_sprites();
        std::size_t recovered_ship_pixels = 0u;
        std::size_t recovered_left_pixels = 0u;
        std::size_t recovered_right_pixels = 0u;
        /* Start lane C0h selects the fourth lateral family: 14 + 3*12. */
        constexpr std::size_t frame_offset = 50u * 26u * 9u;
        for (unsigned x = 0; x < 26u; ++x) {
            for (unsigned y = 0; y < 9u; ++y) {
                const auto color = sprites[frame_offset + x * 9u + y];
                if (color != 0u &&
                    game.pixels()[(103u + y) * 320u + 147u + x] == color) {
                    ++recovered_ship_pixels;
                    if (x < 13u) ++recovered_left_pixels;
                    else ++recovered_right_pixels;
                }
            }
        }
        require(recovered_ship_pixels > 50u,
            "Kosmonaut gameplay no longer draws the recovered ship frame");
        require(recovered_left_pixels > 20u && recovered_right_pixels > 20u,
            "Kosmonaut center seam clipped one half of the recovered ship");
        if (argc > 1) {
            write_frame(argv[1], game);
        }
        if (argc > 2) {
            skyroads::KosmonautGame demo(isolated);
            demo.enter();
            skyroads::NativeInput demo_input;
            demo_input.enter_pressed = true;
            demo.tick(demo_input);
            demo_input = {};
            for (unsigned tick = 0; tick < 1254u; ++tick) demo.tick(demo_input);
            require(demo.playing(),
                "Kosmonaut did not enter its original automatic demonstration");
            write_frame(argv[2], demo);
        }
        if (argc > 3) {
            skyroads::KosmonautGame tutorial(isolated);
            tutorial.enter();
            skyroads::NativeInput tutorial_input;
            tutorial_input.enter_pressed = true;
            tutorial.tick(tutorial_input);
            tutorial_input = {};
            for (unsigned tick = 0; tick < 6u; ++tick) {
                tutorial.tick(tutorial_input);
            }
            const unsigned tutorial_ticks = argc > 4
                ? static_cast<unsigned>(std::stoul(argv[4])) : 95u;
            for (unsigned tick = 0; tick < tutorial_ticks; ++tick) {
                tutorial.tick(tutorial_input);
            }
            write_frame(argv[3], tutorial);
        }
        {
            skyroads::KosmonautGame demo_shortcut(isolated);
            demo_shortcut.enter();
            skyroads::NativeInput shortcut_input;
            shortcut_input.kosmonaut_demo_pressed = true;
            demo_shortcut.tick(shortcut_input);
            shortcut_input = {};
            for (unsigned tick = 0; tick < 6u; ++tick) {
                demo_shortcut.tick(shortcut_input);
            }
            shortcut_input.kosmonaut_demo_pressed = true;
            demo_shortcut.tick(shortcut_input);
            require(demo_shortcut.playing(),
                "Kosmonaut D did not start the original demonstration");
        }
        input = {};
        input.up = true;
        for (unsigned tick = 0; tick < 24u; ++tick) game.tick(input);
        require(game.playing(),
            "Kosmonaut did not sustain a native gameplay session");
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "Kosmonaut test failure: " << error.what() << '\n';
        return 1;
    }
}
