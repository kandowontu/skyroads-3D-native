#include "recovered_game.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::size_t kFrameBytes = 320u * 200u;
constexpr auto kCaptureTicks = [] {
    std::array<std::uint16_t, 1702> ticks{};
    for (std::uint16_t tick = 0; tick < ticks.size(); ++tick) {
        ticks[tick] = tick;
    }
    return ticks;
}();

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("Could not open " + path.string());
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

bool compare_frame(
    const std::vector<std::uint8_t>& actual,
    const std::vector<std::uint8_t>& oracle,
    std::size_t oracle_offset,
    std::uint16_t tick,
    std::uint32_t rendered_distance) {
    if (actual.size() != kFrameBytes) {
        throw std::runtime_error("Native indexed framebuffer has the wrong size");
    }
    std::size_t first_mismatch = kFrameBytes;
    std::size_t mismatch_count = 0;
    std::size_t min_x = 320;
    std::size_t min_y = 200;
    std::size_t max_x = 0;
    std::size_t max_y = 0;
    for (std::size_t offset = 0; offset < kFrameBytes; ++offset) {
        const auto expected = oracle[oracle_offset + offset];
        if (actual[offset] != expected) {
            if (first_mismatch == kFrameBytes) first_mismatch = offset;
            ++mismatch_count;
            const auto x = offset % 320u;
            const auto y = offset / 320u;
            min_x = std::min(min_x, x);
            min_y = std::min(min_y, y);
            max_x = std::max(max_x, x);
            max_y = std::max(max_y, y);
        }
    }
    if (first_mismatch != kFrameBytes) {
        if (mismatch_count <= 32u) {
            for (std::size_t offset = 0; offset < kFrameBytes; ++offset) {
                const auto expected = oracle[oracle_offset + offset];
                if (actual[offset] != expected) {
                    std::cerr << "  (" << offset % 320u << ", " << offset / 320u
                              << ") expected " << static_cast<unsigned>(expected)
                              << ", got " << static_cast<unsigned>(actual[offset]) << '\n';
                }
            }
        }
        else {
            std::map<std::pair<unsigned, unsigned>, std::size_t> pairs;
            for (std::size_t offset = 0; offset < kFrameBytes; ++offset) {
                const unsigned expected = oracle[oracle_offset + offset];
                const unsigned got = actual[offset];
                if (got != expected) ++pairs[{expected, got}];
            }
            std::vector<std::pair<std::size_t, std::pair<unsigned, unsigned>>> ranked;
            for (const auto& [pair, count] : pairs) ranked.push_back({count, pair});
            std::sort(ranked.begin(), ranked.end(), std::greater<>());
            const auto limit = std::min<std::size_t>(ranked.size(), 6u);
            std::cerr << "  most common expected->got pairs:";
            for (std::size_t index = 0; index < limit; ++index) {
                std::cerr << ' ' << ranked[index].second.first << "->"
                          << ranked[index].second.second << " ("
                          << ranked[index].first << ')';
            }
            std::cerr << '\n';
        }
        const auto x = first_mismatch % 320u;
        const auto y = first_mismatch / 320u;
        std::cerr << "VGA framebuffer mismatch at demo tick " << tick << ": "
                  << mismatch_count << " pixels differ in (" << min_x << ", "
                  << min_y << ")-(" << max_x << ", " << max_y
                  << "); first pixel (" << x << ", " << y << ") expected "
                  << static_cast<unsigned>(oracle[oracle_offset + first_mismatch])
                  << ", got " << static_cast<unsigned>(actual[first_mismatch])
                  << "; rendered distance=0x" << std::hex << rendered_distance
                  << std::dec << '\n';
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 3) {
            throw std::runtime_error("usage: dos_frame_oracle_tests DATA_ROOT ORAFRAME.BIN");
        }
        const auto oracle = read_file(argv[2]);
        const auto post_trek_path =
            std::filesystem::path(argv[2]).parent_path() / "ORATRK1P.BIN";
        const auto post_trek = read_file(post_trek_path);
        const auto all_trek_path =
            std::filesystem::path(argv[2]).parent_path() / "ORATREKS.BIN";
        const auto all_trek = read_file(all_trek_path);
        const auto car_221_path =
            std::filesystem::path(argv[2]).parent_path() / "ORACAR43.BIN";
        const auto car_221 = read_file(car_221_path);
        const auto mask_221_path =
            std::filesystem::path(argv[2]).parent_path() / "ORAMSK21.BIN";
        const auto mask_221 = read_file(mask_221_path);
        constexpr std::array<std::size_t, 8> trek_sizes{
            24716, 25775, 26324, 26702, 27278, 26780, 26399, 26153,
        };
        if (oracle.size() != kCaptureTicks.size() * kFrameBytes) {
            throw std::runtime_error("DOS framebuffer oracle has the wrong size");
        }

        skyroads::RecoveredGame game(argv[1]);
        skyroads::NativeInput input{};
        for (unsigned irq = 0;
             irq < 200000 && game.screen() != skyroads::NativeScreen::Demo;
             ++irq) {
            game.timer_tick(input);
        }
        if (game.screen() != skyroads::NativeScreen::Demo) {
            throw std::runtime_error("Recovered intro did not enter the demo");
        }

        unsigned mismatched_frames = 0;
        for (std::size_t frame = 0; frame < kCaptureTicks.size(); ++frame) {
            const auto desired_gameplay_ticks =
                static_cast<std::uint16_t>(kCaptureTicks[frame] + 1u);
            std::uint32_t rendered_distance = game.road_distance();
            for (unsigned irq = 0;
                 irq < 10000 && game.gameplay_ticks() < desired_gameplay_ticks;
                 ++irq) {
                rendered_distance = game.road_distance();
                game.timer_tick(input);
            }
            if (game.gameplay_ticks() != desired_gameplay_ticks) {
                throw std::runtime_error(
                    "Native demo did not reach captured gameplay tick " +
                    std::to_string(kCaptureTicks[frame]));
            }
            if (kCaptureTicks[frame] == 221u) {
                if (game.car_frame_bytes(game.last_ship_frame()) != car_221) {
                    throw std::runtime_error("DOS and native diagnostic car frames differ");
                }
                const auto native_mask = game.ship_mask_bytes();
                if (native_mask != mask_221) {
                    const auto first = std::mismatch(
                        native_mask.begin(), native_mask.end(), mask_221.begin());
                    const auto byte = static_cast<std::size_t>(
                        first.first - native_mask.begin());
                    throw std::runtime_error(
                        "DOS and native diagnostic ship masks differ at byte " +
                        std::to_string(byte) + ": expected " +
                        std::to_string(mask_221[byte]) + ", got " +
                        std::to_string(native_mask[byte]));
                }
            }
            if (!compare_frame(
                    game.indexed_pixels(), oracle, frame * kFrameBytes,
                    kCaptureTicks[frame], rendered_distance)) {
                ++mismatched_frames;
            }
            if (kCaptureTicks[frame] == 15u) {
                const auto native_trek = game.trek_record_bytes(1);
                if (native_trek != post_trek) {
                    std::size_t first = 0;
                    while (first < native_trek.size() && first < post_trek.size() &&
                           native_trek[first] == post_trek[first]) ++first;
                    throw std::runtime_error(
                        "Post-draw TREKDAT phase-1 record differs at byte " +
                        std::to_string(first) + ": expected " +
                        (first < post_trek.size()
                            ? std::to_string(post_trek[first]) : "EOF") +
                        ", got " + (first < native_trek.size()
                            ? std::to_string(native_trek[first]) : "EOF"));
                }
            }
            if (kCaptureTicks[frame] == 1528u) {
                std::size_t record_offset = 0;
                bool trek_mismatch = false;
                for (std::size_t record = 0; record < trek_sizes.size(); ++record) {
                    const auto native_trek = game.trek_record_bytes(record);
                    if (native_trek.size() != trek_sizes[record]) {
                        throw std::runtime_error("Native TREKDAT record has the wrong size");
                    }
                    const auto first = std::mismatch(
                        native_trek.begin(), native_trek.end(),
                        all_trek.begin() + static_cast<std::ptrdiff_t>(record_offset));
                    if (first.first != native_trek.end()) {
                        const auto byte = static_cast<std::size_t>(
                            first.first - native_trek.begin());
                        std::cerr << "Post-frame TREKDAT record " << record
                                  << " differs at byte " << byte << ": expected "
                                  << static_cast<unsigned>(all_trek[record_offset + byte])
                                  << ", got " << static_cast<unsigned>(native_trek[byte])
                                  << '\n';
                        trek_mismatch = true;
                    }
                    record_offset += trek_sizes[record];
                }
                if (record_offset != all_trek.size()) {
                    throw std::runtime_error("DOS all-record TREKDAT trace has the wrong size");
                }
                if (trek_mismatch) {
                    throw std::runtime_error("Post-frame TREKDAT records differ");
                }
            }
        }

        if (mismatched_frames != 0) {
            throw std::runtime_error(
                std::to_string(mismatched_frames) +
                " of " + std::to_string(kCaptureTicks.size()) +
                " captured DOS VGA framebuffers differ");
        }

        std::cout << "Matched " << kCaptureTicks.size()
                  << " DOS VGA framebuffers byte-for-byte\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
