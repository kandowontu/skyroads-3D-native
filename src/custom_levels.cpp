#include "custom_levels.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <system_error>

namespace skyroads {
namespace {

constexpr std::array<std::uint8_t, 8> kMagic{
    'S', 'R', 'L', 'E', 'V', 'E', 'L', '1'};
constexpr std::size_t kMaximumNameBytes = 24u;

void set_error(std::string* error, std::string message) {
    if (error != nullptr) *error = std::move(message);
}

bool read_exact(std::istream& stream, void* destination, std::size_t count) {
    return count == 0 || static_cast<bool>(stream.read(
        static_cast<char*>(destination), static_cast<std::streamsize>(count)));
}

bool read_u16(std::istream& stream, std::uint16_t& value) {
    std::array<std::uint8_t, 2> bytes{};
    if (!read_exact(stream, bytes.data(), bytes.size())) return false;
    value = static_cast<std::uint16_t>(
        bytes[0] | static_cast<std::uint16_t>(bytes[1] << 8u));
    return true;
}

bool read_u32(std::istream& stream, std::uint32_t& value) {
    std::array<std::uint8_t, 4> bytes{};
    if (!read_exact(stream, bytes.data(), bytes.size())) return false;
    value = static_cast<std::uint32_t>(bytes[0]) |
        (static_cast<std::uint32_t>(bytes[1]) << 8u) |
        (static_cast<std::uint32_t>(bytes[2]) << 16u) |
        (static_cast<std::uint32_t>(bytes[3]) << 24u);
    return true;
}

void write_u16(std::ostream& stream, std::uint16_t value) {
    const std::array<char, 2> bytes{
        static_cast<char>(value), static_cast<char>(value >> 8u)};
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

void write_u32(std::ostream& stream, std::uint32_t value) {
    const std::array<char, 4> bytes{
        static_cast<char>(value), static_cast<char>(value >> 8u),
        static_cast<char>(value >> 16u), static_cast<char>(value >> 24u)};
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

bool valid_level(const CustomLevel& level) {
    const auto rows = level.row_count();
    return !level.path.empty() && !level.name.empty() &&
        level.name.size() <= kMaximumNameBytes &&
        rows >= kCustomRoadMinimumRows && rows <= kCustomRoadMaximumRows &&
        level.cells.size() == rows * kCustomRoadColumns &&
        level.gravity != 0u;
}

void set_path_row(
    CustomLevel& level,
    std::size_t row,
    int center,
    unsigned half_width,
    std::uint16_t cell) {
    if (row >= level.row_count()) return;
    for (int column = center - static_cast<int>(half_width);
         column <= center + static_cast<int>(half_width); ++column) {
        if (column >= 0 && column < static_cast<int>(kCustomRoadColumns)) {
            level.cells[row * kCustomRoadColumns +
                static_cast<std::size_t>(column)] = cell;
        }
    }
}

CustomLevel make_skybridge(const std::filesystem::path& path) {
    CustomLevel level;
    level.path = path;
    level.name = "SKYBRIDGE RUN";
    level.theme = 2u;
    level.gravity = 8u;
    level.fuel = 300u;
    level.oxygen = 300u;
    level.cells.assign(112u * kCustomRoadColumns, 0u);
    for (std::size_t row = 0; row < level.row_count(); ++row) {
        const int center = row < 28u ? 3 : row < 52u ? 2 : row < 76u ? 4 : 3;
        set_path_row(level, row, center, row < 12u ? 2u : 1u, 0x0005u);
    }
    for (std::size_t row = 20u; row < 24u; ++row) {
        set_path_row(level, row, 3, 0u, 0x000au);
    }
    for (std::size_t row = 46u; row < 49u; ++row) {
        level.cells[row * kCustomRoadColumns + 2u] = 0u;
    }
    for (std::size_t row = 70u; row < 73u; ++row) {
        level.cells[row * kCustomRoadColumns + 4u] = 0x0009u;
    }
    for (std::size_t row = level.row_count() - 2u;
         row < level.row_count(); ++row) {
        set_path_row(level, row, 3, 1u, 0x010eu);
    }
    return level;
}

CustomLevel make_nebula_slalom(const std::filesystem::path& path) {
    CustomLevel level;
    level.path = path;
    level.name = "NEBULA SLALOM";
    level.theme = 11u;
    level.gravity = 10u;
    level.fuel = 300u;
    level.oxygen = 300u;
    level.cells.assign(132u * kCustomRoadColumns, 0u);
    for (std::size_t row = 0; row < level.row_count(); ++row) {
        const auto phase = (row / 12u) % 4u;
        const int center = phase == 0u ? 3 : phase == 1u ? 2 :
            phase == 2u ? 3 : 4;
        const auto material = row >= 56u && row < 72u
            ? static_cast<std::uint16_t>(0x0008u)
            : static_cast<std::uint16_t>(0x0003u);
        set_path_row(level, row, center, row < 14u ? 2u : 1u, material);
    }
    for (std::size_t row = 32u; row < 38u; ++row) {
        level.cells[row * kCustomRoadColumns + 3u] = 0x0201u;
    }
    for (std::size_t row = 88u; row < 92u; ++row) {
        set_path_row(level, row, 3, 0u, 0x000au);
    }
    for (std::size_t row = 108u; row < 111u; ++row) {
        set_path_row(level, row, 3, 0u, 0x0009u);
    }
    for (std::size_t row = level.row_count() - 2u;
         row < level.row_count(); ++row) {
        set_path_row(level, row, 3, 1u, 0x010eu);
    }
    return level;
}

} // namespace

bool load_custom_level(
    const std::filesystem::path& path,
    CustomLevel& level,
    std::string* error) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        set_error(error, "Unable to open " + path.string());
        return false;
    }
    std::array<std::uint8_t, kMagic.size()> magic{};
    std::uint16_t name_size{};
    std::uint16_t theme{};
    std::uint16_t gravity{};
    std::uint16_t fuel{};
    std::uint16_t oxygen{};
    std::uint32_t rows{};
    if (!read_exact(stream, magic.data(), magic.size()) || magic != kMagic ||
        !read_u16(stream, name_size) || !read_u16(stream, theme) ||
        !read_u16(stream, gravity) || !read_u16(stream, fuel) ||
        !read_u16(stream, oxygen) || !read_u32(stream, rows) ||
        name_size == 0u || name_size > kMaximumNameBytes ||
        rows < kCustomRoadMinimumRows || rows > kCustomRoadMaximumRows) {
        set_error(error, "Invalid custom SkyRoads header in " + path.string());
        return false;
    }
    CustomLevel decoded;
    decoded.path = path;
    decoded.theme = theme;
    decoded.gravity = gravity;
    decoded.fuel = fuel;
    decoded.oxygen = oxygen;
    decoded.name.resize(name_size);
    decoded.cells.resize(static_cast<std::size_t>(rows) * kCustomRoadColumns);
    if (!read_exact(stream, decoded.name.data(), decoded.name.size())) {
        set_error(error, "Truncated custom SkyRoads name in " + path.string());
        return false;
    }
    for (auto& cell : decoded.cells) {
        if (!read_u16(stream, cell)) {
            set_error(error, "Truncated custom SkyRoads cells in " + path.string());
            return false;
        }
    }
    if (!valid_level(decoded)) {
        set_error(error, "Invalid custom SkyRoads values in " + path.string());
        return false;
    }
    level = std::move(decoded);
    return true;
}

bool save_custom_level(const CustomLevel& level, std::string* error) {
    if (!valid_level(level)) {
        set_error(error, "Custom SkyRoads level is incomplete");
        return false;
    }
    std::error_code filesystem_error;
    std::filesystem::create_directories(level.path.parent_path(), filesystem_error);
    if (filesystem_error) {
        set_error(error, "Unable to create " + level.path.parent_path().string());
        return false;
    }
    std::ofstream stream(level.path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        set_error(error, "Unable to write " + level.path.string());
        return false;
    }
    stream.write(reinterpret_cast<const char*>(kMagic.data()),
        static_cast<std::streamsize>(kMagic.size()));
    write_u16(stream, static_cast<std::uint16_t>(level.name.size()));
    write_u16(stream, level.theme);
    write_u16(stream, level.gravity);
    write_u16(stream, level.fuel);
    write_u16(stream, level.oxygen);
    write_u32(stream, static_cast<std::uint32_t>(level.row_count()));
    stream.write(level.name.data(), static_cast<std::streamsize>(level.name.size()));
    for (const auto cell : level.cells) write_u16(stream, cell);
    if (!stream) {
        set_error(error, "Unable to finish " + level.path.string());
        return false;
    }
    return true;
}

std::vector<CustomLevel> load_custom_level_catalog(
    const std::filesystem::path& directory) {
    std::vector<CustomLevel> levels;
    std::error_code error;
    if (!std::filesystem::is_directory(directory, error)) return levels;
    for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
        if (error) break;
        if (!entry.is_regular_file(error) ||
            entry.path().extension() != ".srlevel") continue;
        CustomLevel level;
        if (load_custom_level(entry.path(), level)) levels.push_back(std::move(level));
    }
    std::sort(levels.begin(), levels.end(), [](const auto& left, const auto& right) {
        if (left.name != right.name) return left.name < right.name;
        return left.path.filename() < right.path.filename();
    });
    return levels;
}

CustomLevel make_blank_custom_level(
    std::filesystem::path path,
    std::string name,
    std::uint16_t theme) {
    CustomLevel level;
    level.path = std::move(path);
    level.name = std::move(name);
    level.theme = theme;
    level.cells.assign(96u * kCustomRoadColumns, 0u);
    for (std::size_t row = 0; row < level.row_count(); ++row) {
        set_path_row(level, row, 3, row < 12u ? 2u : 1u, 0x0001u);
    }
    for (std::size_t row = level.row_count() - 2u;
         row < level.row_count(); ++row) {
        set_path_row(level, row, 3, 1u, 0x010eu);
    }
    return level;
}

void ensure_demo_custom_levels(const std::filesystem::path& directory) {
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) return;
    const auto skybridge_path = directory / "demo_skybridge.srlevel";
    const auto slalom_path = directory / "demo_nebula_slalom.srlevel";
    const auto skybridge = make_skybridge(skybridge_path);
    if (!std::filesystem::exists(skybridge_path, error)) {
        (void)save_custom_level(skybridge);
    }
    else {
        CustomLevel installed;
        if (load_custom_level(skybridge_path, installed) &&
            installed.name == skybridge.name && installed.fuel <= 7u &&
            installed.oxygen <= 7u) {
            installed.fuel = skybridge.fuel;
            installed.oxygen = skybridge.oxygen;
            (void)save_custom_level(installed);
        }
    }
    error.clear();
    const auto slalom = make_nebula_slalom(slalom_path);
    if (!std::filesystem::exists(slalom_path, error)) {
        (void)save_custom_level(slalom);
    }
    else {
        CustomLevel installed;
        if (load_custom_level(slalom_path, installed) &&
            installed.name == slalom.name && installed.fuel <= 7u &&
            installed.oxygen <= 7u) {
            installed.fuel = slalom.fuel;
            installed.oxygen = slalom.oxygen;
            (void)save_custom_level(installed);
        }
    }
}

} // namespace skyroads
