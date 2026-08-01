#ifndef SKYROADS_RECOVERED_RENDERER_TABLES_H
#define SKYROADS_RECOVERED_RENDERER_TABLES_H

#include <stddef.h>
#include <stdint.h>

enum {
    SR_SHIP_MASK_WIDTH = 29,
    SR_SHIP_IMAGE_HEIGHT = 24,
    SR_SHIP_SHADOW_HEIGHT = 9,
    SR_SHIP_MASK_HEIGHT = SR_SHIP_IMAGE_HEIGHT + SR_SHIP_SHADOW_HEIGHT,
    SR_SHIP_MASK_SIZE = SR_SHIP_MASK_WIDTH * SR_SHIP_MASK_HEIGHT,
    SR_SHADOW_FRAME_SIZE = SR_SHIP_MASK_WIDTH * SR_SHIP_SHADOW_HEIGHT,
    SR_SHADOW_FRAME_COUNT = 5,
    SR_VISIBILITY_HEIGHT_COUNT = 138
};

typedef struct SrRendererTables {
    uint16_t visibility_half_widths[SR_VISIBILITY_HEIGHT_COUNT];
    uint8_t shadows[SR_SHADOW_FRAME_COUNT][SR_SHADOW_FRAME_SIZE];
} SrRendererTables;

typedef struct SrVgaRendererState {
    /* DOS SS:0E92. The last byte intentionally survives each partial clear. */
    uint8_t ship_mask[SR_SHIP_MASK_SIZE];
    /* Executable-owned VGA work segment at DS:5486 and prior-frame mask data. */
    uint8_t road_buffer[320 * 200];
    uint8_t previous_ship_mask[SR_SHIP_MASK_SIZE];
    uint16_t previous_road_phase;
    uint16_t previous_car_offset;
    uint16_t previous_shadow_offset;
    uint8_t road_buffer_initialized;
    /* Native diagnostic fields; they do not participate in DOS renderer state. */
    uint16_t failure_stage;
    int16_t failure_depth;
    int16_t failure_row;
    int16_t failure_column;
    uint16_t failure_detail;
    uint32_t failure_offset;
} SrVgaRendererState;

/* Extracts the VGA ship visibility and shadow tables at DS:044A and DS:065E. */
int sr_load_renderer_tables_from_exe(
    const uint8_t *bytes,
    size_t size,
    SrRendererTables *tables);

void sr_vga_renderer_state_init(SrVgaRendererState *state);

#endif
