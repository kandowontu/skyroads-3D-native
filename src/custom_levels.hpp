#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace skyroads {

inline constexpr std::size_t kCustomRoadColumns = 7u;
inline constexpr std::size_t kCustomRoadMinimumRows = 16u;
inline constexpr std::size_t kCustomRoadMaximumRows = 2048u;

struct CustomLevel {
    std::filesystem::path path;
    std::string name;
    std::uint16_t theme{};
    std::uint16_t gravity{8u};
    std::uint16_t fuel{300u};
    std::uint16_t oxygen{300u};
    std::vector<std::uint16_t> cells;

    [[nodiscard]] std::size_t row_count() const {
        return cells.size() / kCustomRoadColumns;
    }
};

[[nodiscard]] bool load_custom_level(
    const std::filesystem::path& path,
    CustomLevel& level,
    std::string* error = nullptr);

[[nodiscard]] bool save_custom_level(
    const CustomLevel& level,
    std::string* error = nullptr);

[[nodiscard]] std::vector<CustomLevel> load_custom_level_catalog(
    const std::filesystem::path& directory);

[[nodiscard]] CustomLevel make_blank_custom_level(
    std::filesystem::path path,
    std::string name,
    std::uint16_t theme = 0u);

void ensure_demo_custom_levels(const std::filesystem::path& directory);

} // namespace skyroads
