#include "recovered_game.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::size_t kPaletteBytes = 256u * 3u;
using Palette = std::array<std::uint8_t, kPaletteBytes>;

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("Could not open " + path.string());
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

std::uint16_t read_u16(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    return static_cast<std::uint16_t>(
        bytes[offset] | static_cast<std::uint16_t>(bytes[offset + 1u]) << 8u);
}

std::vector<Palette> decode_changed_palettes(const std::vector<std::uint8_t>& bytes) {
    std::vector<Palette> result;
    Palette palette{};
    Palette previous{};
    std::size_t cursor = 0;
    while (cursor < bytes.size()) {
        if (bytes.size() - cursor < 4u) {
            throw std::runtime_error("DOS palette oracle has a truncated header");
        }
        const auto base = read_u16(bytes, cursor);
        const auto count = read_u16(bytes, cursor + 2u);
        cursor += 4u;
        const auto payload = static_cast<std::size_t>(count) * 3u;
        if (base + count > 256u || payload > bytes.size() - cursor) {
            throw std::runtime_error("DOS palette oracle has an invalid range");
        }
        std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(cursor), payload,
            palette.begin() + static_cast<std::ptrdiff_t>(base * 3u));
        cursor += payload;
        if (palette != previous) {
            result.push_back(palette);
            previous = palette;
        }
    }
    return result;
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 3) {
            throw std::runtime_error("usage: dos_palette_oracle_tests DATA_ROOT ORAPAL.BIN");
        }
        const auto expected = decode_changed_palettes(read_file(argv[2]));
        skyroads::RecoveredGame game(argv[1]);
        skyroads::NativeInput input{};
        game.begin_palette_trace();

        for (unsigned irq = 0; irq < 200000; ++irq) {
            game.timer_tick(input);
            if (game.screen() == skyroads::NativeScreen::Demo) break;
        }
        if (game.screen() != skyroads::NativeScreen::Demo) {
            throw std::runtime_error("Native intro did not reach the demo");
        }
        const auto trace = game.consume_palette_trace();
        if (trace.size() % kPaletteBytes != 0) {
            throw std::runtime_error("Native VGA palette trace has the wrong size");
        }
        std::vector<Palette> actual(trace.size() / kPaletteBytes);
        for (std::size_t event = 0; event < actual.size(); ++event) {
            std::copy_n(
                trace.begin() + static_cast<std::ptrdiff_t>(event * kPaletteBytes),
                kPaletteBytes, actual[event].begin());
        }

        std::size_t native_cursor = 0;
        std::size_t expected_subsequence = 0;
        for (const auto& palette : expected) {
            const auto found = std::find(
                actual.begin() + static_cast<std::ptrdiff_t>(native_cursor),
                actual.end(), palette);
            if (found == actual.end()) break;
            native_cursor = static_cast<std::size_t>(found - actual.begin()) + 1u;
            ++expected_subsequence;
        }
        if (expected_subsequence != expected.size()) {
            const auto event = expected_subsequence;
            const auto actual_event = std::min(native_cursor, actual.size() - 1u);
            const auto anywhere = std::find(
                actual.begin(), actual.end(), expected[event]);
            const auto mismatch = std::mismatch(
                expected[event].begin(), expected[event].end(),
                actual[actual_event].begin());
            const auto byte = static_cast<std::size_t>(
                mismatch.first - expected[event].begin());
            throw std::runtime_error(
                "DOS changed-palette state " + std::to_string(event) +
                " is absent after native state " + std::to_string(actual_event) +
                "; first comparison byte " + std::to_string(byte) +
                " expected " + std::to_string(expected[event][byte]) +
                ", got " + std::to_string(actual[actual_event][byte]) +
                "; state exists at " +
                (anywhere == actual.end() ? std::string("never") :
                    std::to_string(anywhere - actual.begin())) +
                "; counts " + std::to_string(expected.size()) + "/" +
                std::to_string(actual.size()));
        }

        /* The DOS interpolation loop is wall-clock driven: VGA I/O can skip
         * intermediate percentages while the native 36 Hz state machine emits
         * every logical step.  Every state the executable actually displayed
         * must therefore occur byte-for-byte and in order in the native trace. */
        std::cout << "Matched all " << expected.size()
                  << " DOS-observed changed VGA DAC states byte-for-byte and in order"
                  << " across " << actual.size() << " native logical steps\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
