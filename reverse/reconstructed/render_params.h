#ifndef SKYROADS_RECOVERED_RENDER_PARAMS_H
#define SKYROADS_RECOVERED_RENDER_PARAMS_H

#include <stddef.h>
#include <stdint.h>

#include "gameplay.h"

typedef struct SrRoadFrameParams {
    uint16_t finish_gate;
    uint16_t ship_frame;
    uint16_t horizontal_sample;
    uint16_t road_phase;
    uint16_t ship_height_units;
    uint16_t surface_clearance_units;
    uint32_t ship_frame_byte_offset;
} SrRoadFrameParams;

/* Exact small renderer selectors at skyroads.exe 1000:0AFA, 0B34, 0B71. */
uint16_t sr_select_ship_attitude(int16_t vertical_velocity, uint16_t ship_height);
uint16_t sr_road_column_from_horizontal(uint16_t horizontal_position);
uint16_t sr_road_surface_height(
    const uint16_t *cells,
    size_t row_count,
    uint32_t distance,
    uint16_t horizontal_position,
    int finish_gate);

/* Exact parameter preparation performed by skyroads.exe 1000:0BE3. */
void sr_prepare_road_frame(
    const uint16_t *cells,
    size_t row_count,
    const SrGameplayState *state,
    uint16_t gameplay_tick,
    int lower_ship_by_one_unit,
    SrRoadFrameParams *params);

#endif
