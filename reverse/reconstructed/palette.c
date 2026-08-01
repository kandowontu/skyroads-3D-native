#include "palette.h"

#include <string.h>

uint16_t sr_palette_transition_percent(uint16_t elapsed, uint16_t duration) {
    uint32_t value;
    if (duration == 0) return 100;
    value = (uint32_t)elapsed * 100u / duration;
    return (uint16_t)(value > 100u ? 100u : value);
}

int sr_blend_palette_bytes(
    uint8_t *output,
    const uint8_t *from,
    const uint8_t *to,
    unsigned byte_count,
    uint16_t percent) {
    unsigned index;
    if (output == 0 || from == 0 || to == 0) return 0;
    if (percent > 100u) percent = 100u;
    for (index = 0; index < byte_count; ++index) {
        int delta = (int)to[index] - (int)from[index];
        output[index] = (uint8_t)(
            (int)from[index] + delta * (int)percent / 100);
    }
    return 1;
}

int sr_build_gameplay_palette_vga(
    uint8_t palette[SR_VGA_PALETTE_BYTES],
    const SrRoadData *road,
    const SrCarSprites *cars,
    const SrGraphicsArchive *dashboard,
    const SrGraphicsArchive *world) {
    if (palette == 0 || road == 0 || cars == 0 || dashboard == 0 || world == 0 ||
        dashboard->palette_count != SR_WORLD_PALETTE_BASE - SR_DASHBOARD_PALETTE_BASE ||
        world->palette_count != SR_VGA_PALETTE_COLORS - SR_WORLD_PALETTE_BASE) return 0;
    memcpy(palette + SR_ROAD_PALETTE_BASE * 3u,
        road->palette, sizeof(road->palette));
    memcpy(palette + SR_CAR_PALETTE_BASE * 3u,
        cars->palette, sizeof(cars->palette));
    memcpy(palette + SR_DASHBOARD_PALETTE_BASE * 3u,
        dashboard->palette, (size_t)dashboard->palette_count * 3u);
    memcpy(palette + SR_WORLD_PALETTE_BASE * 3u,
        world->palette, (size_t)world->palette_count * 3u);
    return 1;
}
