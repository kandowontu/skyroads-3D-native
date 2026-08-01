#include "render_params.h"

#include "road.h"

static const int16_t horizontal_sample_offsets[7] = {
    -1, -1, -1, 0, 1, 2, 4
};

static const uint16_t animation_offsets[4] = {0, 1, 2, 1};

static const uint16_t road_shape_heights[6] = {
    0x2800, 0x3200, 0x3200, 0x3200, 0x3c00, 0x3c00
};

uint16_t sr_select_ship_attitude(int16_t vertical_velocity, uint16_t ship_height) {
    if (vertical_velocity < -0x162 || ship_height < 0x2800u) {
        return 2;
    }
    if (vertical_velocity < 0x163) {
        return 0;
    }
    return 1;
}

uint16_t sr_road_column_from_horizontal(uint16_t horizontal_position) {
    int16_t scaled = (int16_t)(uint16_t)(horizontal_position / 0x80u - 0x5fu);
    int column = scaled / 0x2e;
    if (column < 0) {
        return 0;
    }
    if (column > 6) {
        return 6;
    }
    return (uint16_t)column;
}

uint16_t sr_road_surface_height(
    const uint16_t *cells,
    size_t row_count,
    uint32_t distance,
    uint16_t horizontal_position,
    int finish_gate) {
    uint16_t cell = sr_road_cell_at_position(
        cells, row_count, distance, horizontal_position);
    uint16_t shape = cell & 0x0f00u;
    uint16_t shape_index = shape >> 8;

    if (shape == 0 || finish_gate) {
        return (cell & 0x000fu) == 0 ? 0 : 0x2800u;
    }
    if (shape == 0x0100u) {
        return 0;
    }
    if (shape_index < 6) {
        return road_shape_heights[shape_index];
    }
    return 0;
}

void sr_prepare_road_frame(
    const uint16_t *cells,
    size_t row_count,
    const SrGameplayState *state,
    uint16_t gameplay_tick,
    int lower_ship_by_one_unit,
    SrRoadFrameParams *params) {
    uint16_t lane;
    uint16_t attitude;
    uint16_t animation;
    uint16_t left_surface;
    uint16_t right_surface;
    uint16_t adjusted_height;

    params->finish_gate = (uint16_t)sr_test_finish_gate(
        cells,
        row_count,
        state->position.distance,
        state->position.horizontal_position,
        state->position.height);
    if (state->result_ticks != 0) {
        params->ship_frame = (uint16_t)(state->result_ticks / 3u);
        if (params->ship_frame >= 14u) {
            params->ship_frame = 0xffffu;
        }
    }
    else {
        animation = state->level_result == SR_LEVEL_OUT_OF_FUEL
            ? 0
            : animation_offsets[(gameplay_tick / 2u) % 4u];
        attitude = params->finish_gate
            ? 0
            : sr_select_ship_attitude(
                state->vertical_velocity, state->position.height);
        lane = sr_road_column_from_horizontal(state->position.horizontal_position);
        params->ship_frame = (uint16_t)(
            (lane * 3u + attitude) * 3u + 14u + animation);
    }

    left_surface = sr_road_surface_height(
        cells,
        row_count,
        state->position.distance,
        (uint16_t)(state->position.horizontal_position - 0x0380u),
        params->finish_gate);
    right_surface = sr_road_surface_height(
        cells,
        row_count,
        state->position.distance,
        (uint16_t)(state->position.horizontal_position + 0x0380u),
        params->finish_gate);
    if (left_surface < right_surface) {
        left_surface = right_surface;
    }
    params->surface_clearance_units = state->result_ticks != 0
        ? 0x7fffu
        : (uint16_t)((uint16_t)(state->position.height - left_surface) / 0x80u);

    adjusted_height = lower_ship_by_one_unit
        ? (uint16_t)(state->position.height - 0x80u)
        : state->position.height;
    params->ship_height_units = params->ship_frame == 0xffffu
        ? 0
        : (uint16_t)(adjusted_height / 0x80u);
    params->road_phase = (uint16_t)(state->position.distance / 0x2000u);
    lane = sr_road_column_from_horizontal(state->position.horizontal_position);
    params->horizontal_sample = (uint16_t)(
        state->position.horizontal_position / 0x80u + horizontal_sample_offsets[lane]);
    params->ship_frame_byte_offset = params->ship_frame == 0xffffu
        ? 0
        : (uint32_t)params->ship_frame * 0x02d0u;
}
