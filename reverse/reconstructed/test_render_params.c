#include "render_params.h"

#include <assert.h>
#include <stdio.h>

static void test_small_selectors(void) {
    uint16_t cells[7] = {0, 0, 0, 0x0001, 0, 0, 0};
    assert(sr_select_ship_attitude(-0x163, 0x2800) == 2);
    assert(sr_select_ship_attitude(-0x162, 0x2800) == 0);
    assert(sr_select_ship_attitude(0x162, 0x2800) == 0);
    assert(sr_select_ship_attitude(0x163, 0x2800) == 1);
    assert(sr_select_ship_attitude(0, 0x27ff) == 2);
    assert(sr_road_column_from_horizontal(0x2f80) == 0);
    assert(sr_road_column_from_horizontal(0x8000) == 3);
    assert(sr_road_column_from_horizontal(0xd080) == 6);
    assert(sr_road_surface_height(cells, 1, 0, 0x8000, 0) == 0x2800);
    assert(sr_road_surface_height(cells, 1, 0, 0x8000, 1) == 0x2800);
}

static void test_frame_parameters(void) {
    uint16_t cells[8 * 7];
    SrGameplayState state = {0};
    SrRoadFrameParams params;
    size_t index;

    for (index = 0; index < sizeof(cells) / sizeof(cells[0]); ++index) {
        cells[index] = 1;
    }
    state.position.distance = 0x00030000u;
    state.position.horizontal_position = 0x8000u;
    state.position.height = 0x2800u;
    sr_prepare_road_frame(cells, 8, &state, 0, 0, &params);
    assert(params.finish_gate == 0);
    assert(params.ship_frame == 41);
    assert(params.ship_frame_byte_offset == 41u * 0x2d0u);
    assert(params.horizontal_sample == 0x100u);
    assert(params.road_phase == 0x18u);
    assert(params.ship_height_units == 0x50u);
    assert(params.surface_clearance_units == 0);

    state.result_ticks = 41;
    sr_prepare_road_frame(cells, 8, &state, 0, 1, &params);
    assert(params.ship_frame == 13);
    assert(params.ship_height_units == 0x4fu);
    assert(params.surface_clearance_units == 0x7fffu);
    state.result_ticks = 42;
    sr_prepare_road_frame(cells, 8, &state, 0, 0, &params);
    assert(params.ship_frame == 0xffffu);
    assert(params.ship_height_units == 0);
    assert(params.ship_frame_byte_offset == 0);
}

int main(void) {
    test_small_selectors();
    test_frame_parameters();
    puts("renderer parameter tests passed");
    return 0;
}
