#include "gameplay.h"
#include "input.h"
#include "road_archive.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct TraceSound {
    uint64_t hash;
    size_t count;
} TraceSound;

static uint8_t *read_file(const char *path, size_t *size) {
    FILE *stream = fopen(path, "rb");
    long length;
    uint8_t *bytes;
    if (stream == 0) return 0;
    fseek(stream, 0, SEEK_END);
    length = ftell(stream);
    fseek(stream, 0, SEEK_SET);
    bytes = (uint8_t *)malloc((size_t)length);
    if (bytes == 0 || fread(bytes, 1, (size_t)length, stream) != (size_t)length) {
        fclose(stream);
        free(bytes);
        return 0;
    }
    fclose(stream);
    *size = (size_t)length;
    return bytes;
}

static uint64_t hash_byte(uint64_t hash, uint8_t value) {
    hash ^= value;
    return hash * UINT64_C(1099511628211);
}

static uint64_t hash_u16(uint64_t hash, uint16_t value) {
    hash = hash_byte(hash, (uint8_t)value);
    return hash_byte(hash, (uint8_t)(value >> 8));
}

static uint64_t hash_u32(uint64_t hash, uint32_t value) {
    hash = hash_u16(hash, (uint16_t)value);
    return hash_u16(hash, (uint16_t)(value >> 16));
}

static uint64_t hash_state(uint64_t hash, const SrGameplayState *state) {
    hash = hash_u32(hash, state->position.distance);
    hash = hash_u16(hash, state->position.horizontal_position);
    hash = hash_u16(hash, state->position.height);
    hash = hash_u32(hash, (uint32_t)state->forward_speed);
    hash = hash_u16(hash, (uint16_t)state->lateral_velocity);
    hash = hash_u16(hash, (uint16_t)state->vertical_velocity);
    hash = hash_u16(hash, (uint16_t)state->surface_impulse);
    hash = hash_u16(hash, state->fuel);
    hash = hash_u16(hash, state->oxygen);
    hash = hash_u16(hash, state->level_result);
    hash = hash_u16(hash, state->result_delay_ticks);
    hash = hash_u16(hash, state->result_ticks);
    hash = hash_u16(hash, state->collision_adjusted);
    hash = hash_u32(hash, (uint32_t)state->collision_speed_correction);
    hash = hash_u16(hash, state->process_surface_cell);
    hash = hash_u16(hash, state->on_kind_8);
    hash = hash_u16(hash, state->on_kind_2);
    hash = hash_u16(hash, state->jumping);
    hash = hash_u16(hash, state->prediction_already_run);
    return hash;
}

static void trace_sound(void *context, unsigned effect) {
    TraceSound *sound = (TraceSound *)context;
    sound->hash = hash_u16(sound->hash, (uint16_t)effect);
    ++sound->count;
}

int main(int argc, char **argv) {
    uint8_t *road_bytes;
    uint8_t *demo_bytes;
    size_t road_size;
    size_t demo_size;
    SrRoadArchive archive;
    SrRoadData *road;
    SrGameplayConfig config;
    SrGameplayState state;
    SrGameplayControls controls;
    SrLevelInput input;
    TraceSound sound = {UINT64_C(1469598103934665603), 0};
    SrGameplayHooks hooks = {&sound, trace_sound, 0};
    uint64_t hash = UINT64_C(1469598103934665603);
    unsigned ticks;
    int finished = 0;

    if (argc != 3) return 1;
    road_bytes = read_file(argv[1], &road_size);
    demo_bytes = read_file(argv[2], &demo_size);
    if (road_bytes == 0 || demo_bytes == 0 ||
        sr_load_road_archive(road_bytes, road_size, &archive) != SR_ROAD_ARCHIVE_OK) {
        free(road_bytes);
        free(demo_bytes);
        return 1;
    }
    free(road_bytes);
    road = &archive.roads[0];
    memset(&config, 0, sizeof(config));
    config.road_gravity = road->gravity;
    config.road_fuel = road->fuel;
    config.road_oxygen = road->oxygen;
    config.road_length_rows = (uint16_t)road->row_count;
    config.collision_resolution_enabled = 1;
    sr_gameplay_init(&state, &config);

    memset(&input, 0, sizeof(input));
    input.mode = SR_INPUT_DEMO;
    input.demo_record = demo_bytes;
    input.demo_record_size = demo_size;
    for (ticks = 0; ticks < 10000; ++ticks) {
        input.road_distance = state.position.distance;
        if (!sr_sample_level_input(&input, 0)) break;
        controls.steering = input.steering;
        controls.throttle = input.throttle;
        controls.jump = input.jump;
        if (sr_gameplay_tick(
                road->cells, road->row_count, &config, &controls, &hooks, &state) ==
            SR_GAMEPLAY_TICK_FINISHED) {
            finished = 1;
            break;
        }
        hash = hash_state(hash, &state);
        if (sr_gameplay_result_ready(&state)) break;
    }
    printf(
        "demo trace: ticks=%u finished=%d result=%u distance=%08lx x=%04x height=%04x "
        "state_hash=%016llx sounds=%zu sound_hash=%016llx\n",
        ticks,
        finished,
        state.level_result,
        (unsigned long)state.position.distance,
        state.position.horizontal_position,
        state.position.height,
        (unsigned long long)hash,
        sound.count,
        (unsigned long long)sound.hash);
    if (ticks != 1702 || !finished || state.level_result != SR_LEVEL_COMPLETE ||
        state.position.distance != UINT32_C(0x009f9127) ||
        state.position.horizontal_position != 0x8078u ||
        state.position.height != 0x2800u ||
        hash != UINT64_C(0x3dbe40d5a5dc7305) || sound.count != 65 ||
        sound.hash != UINT64_C(0xa50984126284315c)) {
        finished = 0;
    }
    sr_free_road_archive(&archive);
    free(demo_bytes);
    return finished ? 0 : 1;
}
