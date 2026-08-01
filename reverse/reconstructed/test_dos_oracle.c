#include "gameplay.h"
#include "input.h"
#include "road_archive.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { ORACLE_RECORD_BYTES = 44 };

typedef struct SoundOracle {
    const uint8_t *bytes;
    size_t size;
    size_t cursor;
    size_t actual_count;
    uint16_t current_tick;
    uint16_t last_sound_tick;
    int mismatch;
} SoundOracle;

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

static void put_u16(uint8_t **cursor, uint16_t value) {
    *(*cursor)++ = (uint8_t)value;
    *(*cursor)++ = (uint8_t)(value >> 8);
}

static void put_u32(uint8_t **cursor, uint32_t value) {
    put_u16(cursor, (uint16_t)value);
    put_u16(cursor, (uint16_t)(value >> 16));
}

static void serialize_state(uint8_t output[ORACLE_RECORD_BYTES], const SrGameplayState *state) {
    uint8_t *cursor = output;
    put_u32(&cursor, state->position.distance);
    put_u16(&cursor, state->position.horizontal_position);
    put_u16(&cursor, state->position.height);
    put_u32(&cursor, (uint32_t)state->forward_speed);
    put_u16(&cursor, (uint16_t)state->lateral_velocity);
    put_u16(&cursor, (uint16_t)state->vertical_velocity);
    put_u16(&cursor, (uint16_t)state->surface_impulse);
    put_u16(&cursor, state->fuel);
    put_u16(&cursor, state->oxygen);
    put_u16(&cursor, state->level_result);
    put_u16(&cursor, state->result_delay_ticks);
    put_u16(&cursor, state->result_ticks);
    put_u16(&cursor, state->collision_adjusted);
    put_u32(&cursor, (uint32_t)state->collision_speed_correction);
    put_u16(&cursor, state->process_surface_cell);
    put_u16(&cursor, state->on_kind_8);
    put_u16(&cursor, state->on_kind_2);
    put_u16(&cursor, state->jumping);
    put_u16(&cursor, state->prediction_already_run);
}

static void compare_sound(void *context, unsigned effect) {
    SoundOracle *oracle = (SoundOracle *)context;
    uint16_t expected;
    size_t index = oracle->actual_count++;
    if (oracle->cursor + 2 > oracle->size) {
        if (!oracle->mismatch) {
            fprintf(stderr, "DOS sound oracle exhausted at event %zu: native=%u\n",
                index, effect);
        }
        oracle->mismatch = 1;
        return;
    }
    expected = (uint16_t)(oracle->bytes[oracle->cursor] |
        ((uint16_t)oracle->bytes[oracle->cursor + 1] << 8));
    if (expected != effect) {
        if (!oracle->mismatch) {
            fprintf(stderr,
                "DOS sound oracle mismatch at event %zu: native=%u DOS=%u\n",
                index, effect, expected);
        }
        oracle->mismatch = 1;
    }
    oracle->cursor += 2;
    oracle->last_sound_tick = oracle->current_tick;
}

static int compare_sound_is_active(void *context) {
    SoundOracle *oracle = (SoundOracle *)context;
    return oracle->current_tick < (uint16_t)(oracle->last_sound_tick + 8u);
}

int main(int argc, char **argv) {
    uint8_t *road_bytes;
    uint8_t *demo_bytes;
    uint8_t *state_bytes;
    uint8_t *sound_bytes;
    size_t road_size, demo_size, state_size, sound_size;
    SrRoadArchive archive;
    SrRoadData *road;
    SrGameplayConfig config;
    SrGameplayState state;
    SrGameplayControls controls;
    SrLevelInput input;
    SoundOracle sound;
    SrGameplayHooks hooks;
    unsigned tick;
    int finished = 0;

    if (argc != 5) return 1;
    road_bytes = read_file(argv[1], &road_size);
    demo_bytes = read_file(argv[2], &demo_size);
    state_bytes = read_file(argv[3], &state_size);
    sound_bytes = read_file(argv[4], &sound_size);
    if (road_bytes == 0 || demo_bytes == 0 || state_bytes == 0 || sound_bytes == 0 ||
        state_size % ORACLE_RECORD_BYTES != 0 ||
        sr_load_road_archive(road_bytes, road_size, &archive) != SR_ROAD_ARCHIVE_OK) {
        free(road_bytes); free(demo_bytes); free(state_bytes); free(sound_bytes);
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
    sound.bytes = sound_bytes;
    sound.size = sound_size;
    sound.cursor = 0;
    sound.actual_count = 0;
    sound.current_tick = 0;
    sound.last_sound_tick = 0;
    sound.mismatch = 0;
    hooks.context = &sound;
    hooks.play_sound = compare_sound;
    hooks.sound_is_active = compare_sound_is_active;

    for (tick = 0; tick < state_size / ORACLE_RECORD_BYTES + 1; ++tick) {
        uint8_t actual[ORACLE_RECORD_BYTES];
        sound.current_tick = (uint16_t)(tick == 0 ? 1 : tick);
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
        if ((size_t)tick * ORACLE_RECORD_BYTES >= state_size) {
            fprintf(stderr, "native trace exceeds DOS oracle at tick %u\n", tick);
            break;
        }
        serialize_state(actual, &state);
        if (memcmp(actual, state_bytes + (size_t)tick * ORACLE_RECORD_BYTES,
                ORACLE_RECORD_BYTES) != 0) {
            unsigned byte;
            for (byte = 0; byte < ORACLE_RECORD_BYTES; ++byte) {
                if (actual[byte] != state_bytes[(size_t)tick * ORACLE_RECORD_BYTES + byte]) {
                    fprintf(stderr,
                        "DOS oracle mismatch at tick %u, record byte %u: native=%02x DOS=%02x\n",
                        tick, byte, actual[byte],
                        state_bytes[(size_t)tick * ORACLE_RECORD_BYTES + byte]);
                    break;
                }
            }
            break;
        }
    }

    if (!finished || tick != state_size / ORACLE_RECORD_BYTES ||
        sound.mismatch || sound.cursor != sound.size) {
        fprintf(stderr,
            "oracle summary: ticks=%u/%zu finished=%d sounds=%zu actual=%zu/%zu mismatch=%d\n",
            tick, state_size / ORACLE_RECORD_BYTES, finished,
            sound.cursor / 2, sound.actual_count, sound.size / 2, sound.mismatch);
        finished = 0;
    }
    else {
        printf("DOS oracle matched %u gameplay ticks and %zu sound events\n",
            tick, sound.size / 2);
    }
    sr_free_road_archive(&archive);
    free(demo_bytes); free(state_bytes); free(sound_bytes);
    return finished ? 0 : 1;
}
