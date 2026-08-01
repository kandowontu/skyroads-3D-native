#ifndef SKYROADS_RECOVERED_PALETTE_H
#define SKYROADS_RECOVERED_PALETTE_H

#include <stdint.h>

#include "car_sprites.h"
#include "graphics_archive.h"
#include "road_archive.h"

enum {
    SR_VGA_PALETTE_COLORS = 256,
    SR_VGA_PALETTE_BYTES = SR_VGA_PALETTE_COLORS * 3,
    SR_ROAD_PALETTE_BASE = 0x00,
    SR_CAR_PALETTE_BASE = 0x48,
    SR_DASHBOARD_PALETTE_BASE = 0x5c,
    SR_WORLD_PALETTE_BASE = 0x8e
};

/* Exact integer percentage and byte interpolation from 1000:4315. */
uint16_t sr_palette_transition_percent(uint16_t elapsed, uint16_t duration);
int sr_blend_palette_bytes(
    uint8_t *output,
    const uint8_t *from,
    const uint8_t *to,
    unsigned byte_count,
    uint16_t percent);

/*
 * Builds the gameplay DAC image from the exact palette bases used by
 * load_road (55F8), load_graphics_archives (554B), and load_world (536B).
 * Components occupy 72 + 20 + 50 + 114 colors, exactly filling VGA's DAC.
 */
int sr_build_gameplay_palette_vga(
    uint8_t palette[SR_VGA_PALETTE_BYTES],
    const SrRoadData *road,
    const SrCarSprites *cars,
    const SrGraphicsArchive *dashboard,
    const SrGraphicsArchive *world);

#endif
