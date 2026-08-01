#ifndef SKYROADS_RECOVERED_MOTION_H
#define SKYROADS_RECOVERED_MOTION_H

#include "road.h"

#include <stddef.h>
#include <stdint.h>

typedef struct SrMotionState {
    SrShipPosition position;
    int32_t forward_speed;              /* DS:54B8:54BA */
    int16_t lateral_velocity;           /* DS:4576 */
    int16_t vertical_velocity;          /* DS:9342 */
    int16_t surface_impulse;            /* DS:54A2 */
    int16_t gravity_step;               /* DS:54B6 */
    int16_t throttle;                   /* DS:933C */
    int32_t collision_speed_correction; /* DS:AF3E:AF40 */
    uint16_t collision_adjusted;        /* DS:4568 */
    /* Native extension; zero preserves the recovered 0x2aaa ceiling. */
    int32_t forward_speed_limit;
} SrMotionState;

/* Exact semantic reconstruction of skyroads.exe 1000:1C20. */
int sr_predict_collision_motion(
    const uint16_t *cells,
    size_t row_count,
    const SrMotionState *state);

/* Exact search order and increments from skyroads.exe 1000:1D4D. */
void sr_resolve_collision_motion(
    const uint16_t *cells,
    size_t row_count,
    SrMotionState *state);

#endif
