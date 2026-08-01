#ifndef SKYROADS_RECOVERED_GAMEPLAY_H
#define SKYROADS_RECOVERED_GAMEPLAY_H

#include <stddef.h>
#include <stdint.h>

#include "road.h"

typedef enum SrLevelResult {
    SR_LEVEL_COMPLETE = 0,
    SR_LEVEL_CRASH = 1,
    SR_LEVEL_TERMINAL = 2,
    SR_LEVEL_FELL = 3,
    SR_LEVEL_OUT_OF_FUEL = 4,
    SR_LEVEL_OUT_OF_OXYGEN = 5,
    SR_LEVEL_ABORTED = 7
} SrLevelResult;

typedef struct SrGameplayConfig {
    uint16_t road_gravity;                 /* DS:456E */
    uint16_t road_oxygen;                  /* DS:4574 */
    uint16_t road_fuel;                    /* DS:54AE */
    uint16_t road_length_rows;             /* DS:41CE */
    uint16_t collision_resolution_enabled; /* DS:457E */
    uint16_t input_lock_active;             /* DS:41CC */
    /* Native extension; zero preserves the executable's 0x2aaa ceiling. */
    int32_t forward_speed_limit;
} SrGameplayConfig;

typedef struct SrGameplayControls {
    int16_t steering; /* DS:9600 */
    int16_t throttle; /* DS:933C */
    uint16_t jump;    /* DS:5488 */
} SrGameplayControls;

typedef struct SrGameplayHooks {
    void *context;
    void (*play_sound)(void *context, unsigned effect);
    int (*sound_is_active)(void *context);
} SrGameplayHooks;

typedef struct SrGameplayState {
    SrShipPosition position;
    int32_t forward_speed;
    int16_t lateral_velocity;
    int16_t vertical_velocity;
    int16_t gravity_step;
    int16_t surface_impulse;
    uint16_t fuel;
    uint16_t oxygen;
    uint16_t level_result;
    uint16_t result_delay_ticks;
    uint16_t result_ticks;
    uint16_t collision_adjusted;
    int32_t collision_speed_correction;
    uint32_t demo_elapsed_ticks;

    /* Persistent automatic locals in skyroads.exe 1000:1F2C. */
    uint16_t process_surface_cell;       /* BP-0C */
    uint16_t on_kind_8;                  /* BP-0E */
    uint16_t on_kind_2;                  /* BP-10 */
    uint16_t jumping;                    /* BP-08 */
    uint16_t prediction_already_run;     /* BP-06 */
    uint16_t jump_start_height;          /* BP-0A */
    uint16_t support_distance;           /* BP-18 */
    uint16_t previous_target_height;     /* BP-1C */
} SrGameplayState;

typedef enum SrGameplayTickResult {
    SR_GAMEPLAY_TICK_RUNNING = 0,
    SR_GAMEPLAY_TICK_FINISHED = 1
} SrGameplayTickResult;

/* Exact initialization sequence at skyroads.exe 1000:1F47-2004. */
void sr_gameplay_init(SrGameplayState *state, const SrGameplayConfig *config);

/*
 * One iteration of the fixed-timer inner loop at 1000:22A3-2ADD.
 * Rendering, device sampling, pause handling, and the finish animation remain
 * in the outer platform loop; all gameplay state transitions are performed here.
 */
SrGameplayTickResult sr_gameplay_tick(
    const uint16_t *cells,
    size_t row_count,
    const SrGameplayConfig *config,
    const SrGameplayControls *controls,
    const SrGameplayHooks *hooks,
    SrGameplayState *state);

/* Exact delayed-result predicate at skyroads.exe 1000:2200-2249. */
int sr_gameplay_result_ready(const SrGameplayState *state);

#endif
