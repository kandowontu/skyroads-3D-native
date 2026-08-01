#include "gameplay.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "physics.h"

typedef struct SoundLog {
    unsigned effects[16];
    size_t count;
    int active;
} SoundLog;

static void record_sound(void *context, unsigned effect) {
    SoundLog *log = (SoundLog *)context;
    assert(log->count < sizeof(log->effects) / sizeof(log->effects[0]));
    log->effects[log->count++] = effect;
}

static int sound_active(void *context) {
    return ((SoundLog *)context)->active;
}

static SrGameplayConfig base_config(void) {
    SrGameplayConfig config;
    memset(&config, 0, sizeof(config));
    config.road_gravity = 3;
    config.road_oxygen = 10;
    config.road_fuel = 10;
    config.road_length_rows = 64;
    return config;
}

static void test_initialization(void) {
    SrGameplayConfig config = base_config();
    SrGameplayState state;

    sr_gameplay_init(&state, &config);
    assert(state.position.distance == 0x00030000u);
    assert(state.position.horizontal_position == 0x8000u);
    assert(state.position.height == 0x2800u);
    assert(state.gravity_step == sr_gravity_step(config.road_gravity));
    assert(state.fuel == 30000);
    assert(state.oxygen == 30000);
    assert(state.process_surface_cell == 1);
}

static void test_ground_tick_and_jump(void) {
    uint16_t cells[64 * 7];
    SrGameplayConfig config = base_config();
    SrGameplayControls controls = {1, 1, 0};
    SrGameplayState state;
    SrGameplayHooks hooks = {0};

    for (size_t i = 0; i < sizeof(cells) / sizeof(cells[0]); ++i) {
        cells[i] = 1;
    }
    sr_gameplay_init(&state, &config);
    assert(sr_gameplay_tick(cells, 64, &config, &controls, &hooks, &state) ==
        SR_GAMEPLAY_TICK_RUNNING);
    assert(state.forward_speed == 75);
    assert(state.lateral_velocity == 0x1d);
    assert(state.position.height == 0x2800u);
    assert(state.process_surface_cell == 1);

    controls.jump = 1;
    assert(sr_gameplay_tick(cells, 64, &config, &controls, &hooks, &state) ==
        SR_GAMEPLAY_TICK_RUNNING);
    assert(state.jumping == 1);
    assert(state.vertical_velocity > 0);
    assert(state.position.height > 0x2800u);
}

static void test_demo_timeout_and_delay(void) {
    uint16_t cells[64 * 7];
    SrGameplayConfig config = base_config();
    SrGameplayControls controls = {0};
    SrGameplayState state;
    SoundLog log = {{0}, 0, 0};
    SrGameplayHooks hooks = {&log, record_sound, sound_active};

    for (size_t i = 0; i < sizeof(cells) / sizeof(cells[0]); ++i) {
        cells[i] = 1;
    }
    config.input_lock_active = 1;
    sr_gameplay_init(&state, &config);
    state.demo_elapsed_ticks = 0x90;
    sr_gameplay_tick(cells, 64, &config, &controls, &hooks, &state);
    assert(state.level_result == SR_LEVEL_TERMINAL);
    assert(state.result_ticks == 2);
    assert(state.result_delay_ticks == 1);
    assert(log.count == 1 && log.effects[0] == 0);

    state.result_ticks = 0x2a;
    assert(!sr_gameplay_result_ready(&state));
    state.result_ticks = 0x2b;
    assert(sr_gameplay_result_ready(&state));
}

static void test_resource_result_priority(void) {
    uint16_t cells[64 * 7];
    SrGameplayConfig config = base_config();
    SrGameplayControls controls = {0};
    SrGameplayState state;

    for (size_t i = 0; i < sizeof(cells) / sizeof(cells[0]); ++i) {
        cells[i] = 1;
    }
    sr_gameplay_init(&state, &config);
    state.fuel = 0;
    state.oxygen = 0;
    sr_gameplay_tick(cells, 64, &config, &controls, 0, &state);
    assert(state.level_result == SR_LEVEL_OUT_OF_OXYGEN);
}

static void test_native_overdrive_limit(void) {
    uint16_t cells[256 * 7];
    SrGameplayConfig config = base_config();
    SrGameplayControls controls = {0, 1, 0};
    SrGameplayState state;

    for (size_t i = 0; i < sizeof(cells) / sizeof(cells[0]); ++i) {
        cells[i] = 1;
    }
    config.road_length_rows = 0;
    config.road_fuel = 0;
    config.road_oxygen = 0;
    config.forward_speed_limit = 0x5554;
    sr_gameplay_init(&state, &config);
    for (unsigned tick = 0; tick < 400; ++tick) {
        sr_gameplay_tick(cells, 256, &config, &controls, 0, &state);
    }
    assert(state.forward_speed == 0x5554);
}

int main(void) {
    test_initialization();
    test_ground_tick_and_jump();
    test_demo_timeout_and_delay();
    test_resource_result_priority();
    test_native_overdrive_limit();
    puts("gameplay tests passed");
    return 0;
}
