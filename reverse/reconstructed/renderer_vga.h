#ifndef SKYROADS_RECOVERED_RENDERER_VGA_H
#define SKYROADS_RECOVERED_RENDERER_VGA_H

#include <stddef.h>
#include <stdint.h>

#include "car_sprites.h"
#include "render_params.h"
#include "renderer_tables.h"
#include "trek_archive.h"

enum {
    SR_VGA_WIDTH = 320,
    SR_VGA_HEIGHT = 200,
    SR_VGA_FRAMEBUFFER_SIZE = SR_VGA_WIDTH * SR_VGA_HEIGHT
};

/*
 * Direct linear-framebuffer form of the VGA draw path at 1000:2D03-3461.
 * It preserves the DOS TREKDAT traversal, descriptor dispatch order, shape
 * mutation, palette-index mapping, span directions, ship occlusion mask, and
 * five-frame road-shadow transform.
 */
int sr_draw_road_scene_vga(
    SrTrekArchive *trek,
    const uint16_t *cells,
    size_t row_count,
    const SrRoadFrameParams *params,
    const SrCarSprites *cars,
    const SrRendererTables *tables,
    SrVgaRendererState *state,
    const uint8_t background[SR_VGA_FRAMEBUFFER_SIZE],
    uint8_t framebuffer[SR_VGA_FRAMEBUFFER_SIZE]);

/* Exact full-copy branch at 1000:3610 for the VGA road viewport. */
int sr_restore_road_viewport_vga(
    uint8_t framebuffer[SR_VGA_FRAMEBUFFER_SIZE],
    const uint8_t background[SR_VGA_FRAMEBUFFER_SIZE]);

#endif
