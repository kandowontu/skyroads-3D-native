#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace skyroads {

class DataError final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct Rgb {
    std::uint8_t r{};
    std::uint8_t g{};
    std::uint8_t b{};
};

struct CompressionWidths {
    std::uint8_t count{};
    std::uint8_t short_distance{};
    std::uint8_t long_distance{};
};

struct DecompressedStream {
    std::vector<std::uint8_t> bytes;
    std::size_t consumed{};
};

DecompressedStream decompress_lzs(
    std::span<const std::uint8_t> source,
    std::size_t offset,
    std::optional<std::size_t> expected_size,
    CompressionWidths widths);

struct ImagePalette {
    std::vector<Rgb> colors;
    std::vector<std::uint8_t> auxiliary;
};

struct ImageFrame {
    std::uint16_t screen_offset{};
    std::uint16_t width{};
    std::uint16_t height{};
    std::size_t palette_index{};
    std::vector<std::uint8_t> pixels;

    [[nodiscard]] int x_offset() const { return static_cast<int>(screen_offset % 320); }
    [[nodiscard]] int y_offset() const { return static_cast<int>(screen_offset / 320); }
};

struct ImageArchive {
    std::vector<ImagePalette> palettes;
    std::vector<ImageFrame> fragments;
    std::vector<std::vector<std::size_t>> frames;
    std::optional<std::uint16_t> declared_animation_frames;
};

ImageArchive load_image_archive(const std::filesystem::path& path);

struct Road {
    std::uint16_t gravity{};
    std::uint16_t fuel{};
    std::uint16_t oxygen{};
    std::array<Rgb, 72> palette{};
    std::vector<std::array<std::uint16_t, 7>> rows;
};

struct RoadArchive {
    std::vector<Road> roads;
};

RoadArchive load_road_archive(const std::filesystem::path& path);

std::filesystem::path find_data_file(
    const std::filesystem::path& directory,
    const std::string& filename);

struct Assets {
    std::filesystem::path source_root;
    ImageArchive intro;
    ImageArchive main_menu;
    ImageArchive dashboard;
    ImageArchive cars;
    std::array<ImageArchive, 10> worlds;
    RoadArchive roads;

    static Assets load(const std::filesystem::path& source_root);
};

} // namespace skyroads
