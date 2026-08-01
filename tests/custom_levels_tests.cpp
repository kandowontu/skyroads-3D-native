#include "custom_levels.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

struct TemporaryDirectory {
    std::filesystem::path path;
    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
};

} // namespace

int main() {
    try {
        const auto unique = std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
        TemporaryDirectory temporary{
            std::filesystem::temp_directory_path() /
                ("skyroads_custom_levels_" + unique)};

        skyroads::ensure_demo_custom_levels(temporary.path);
        auto catalog = skyroads::load_custom_level_catalog(temporary.path);
        require(catalog.size() == 2u,
            "The two bundled custom demo roads were not created");
        require(catalog[0].row_count() >= skyroads::kCustomRoadMinimumRows &&
                catalog[1].row_count() >= skyroads::kCustomRoadMinimumRows,
            "A bundled custom demo road is too short");
        require(catalog[0].fuel >= 100u && catalog[0].oxygen >= 100u &&
                catalog[1].fuel >= 100u && catalog[1].oxygen >= 100u,
            "A bundled demo can exhaust its resources during a normal run");
        catalog[0].fuel = 6u;
        catalog[0].oxygen = 6u;
        require(skyroads::save_custom_level(catalog[0]),
            "Could not create a legacy low-resource demo fixture");
        skyroads::ensure_demo_custom_levels(temporary.path);
        catalog = skyroads::load_custom_level_catalog(temporary.path);
        require(catalog[0].fuel >= 100u && catalog[0].oxygen >= 100u,
            "Installed demo roads were not migrated to safe resource budgets");

        auto original = skyroads::make_blank_custom_level(
            temporary.path / "roundtrip.srlevel", "ROUNDTRIP", 7u);
        original.gravity = 12u;
        original.fuel = 9u;
        original.oxygen = 6u;
        original.cells[3u * skyroads::kCustomRoadColumns + 4u] = 0x030au;
        std::string error;
        require(skyroads::save_custom_level(original, &error), error);

        skyroads::CustomLevel decoded;
        require(skyroads::load_custom_level(original.path, decoded, &error), error);
        require(decoded.name == original.name && decoded.theme == original.theme &&
                decoded.gravity == original.gravity &&
                decoded.fuel == original.fuel &&
                decoded.oxygen == original.oxygen &&
                decoded.cells == original.cells,
            "Custom road metadata or exact 16-bit cells changed after save/load");

        catalog = skyroads::load_custom_level_catalog(temporary.path);
        require(catalog.size() == 3u,
            "The creation browser catalog omitted a saved custom road");
        std::cout << "Custom road catalog and exact-cell roundtrip passed\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
