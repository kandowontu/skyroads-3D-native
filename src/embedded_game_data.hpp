#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace skyroads {

enum class EmbeddedCampaign {
    SkyRoads,
    SkyRoadsXmas,
};

/* Implemented by the build-generated translation unit. The original archives
   are build inputs only and are never added to source control. */
[[nodiscard]] std::span<const std::uint8_t> embedded_game_file(
    EmbeddedCampaign campaign,
    std::string_view name);
[[nodiscard]] bool embedded_xmas_data_available();

} // namespace skyroads
