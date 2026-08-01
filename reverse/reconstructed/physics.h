#ifndef SKYROADS_RECOVERED_PHYSICS_H
#define SKYROADS_RECOVERED_PHYSICS_H

#include <stdint.h>

/* Scalar fixed-point operations extracted from gameplay_loop (1000:1F2C). */
int16_t sr_gravity_step(uint16_t road_gravity);
int32_t sr_adjust_forward_speed(int32_t speed, int16_t throttle);
/* Native extension: the recovered DOS path passes 0x2aaa. */
int32_t sr_adjust_forward_speed_with_limit(
    int32_t speed,
    int16_t throttle,
    int32_t maximum_speed);
int16_t sr_apply_airborne_gravity(
    uint16_t ship_height,
    int16_t vertical_velocity,
    int16_t gravity_step);
uint16_t sr_consume_oxygen(uint16_t oxygen, uint16_t road_oxygen);
uint16_t sr_consume_fuel(uint16_t fuel, uint16_t road_fuel, int32_t forward_speed);

typedef struct SrCellEffectState {
    int32_t forward_speed;       /* DS:54B8:54BA */
    uint16_t fuel;               /* DS:54A0 */
    uint16_t oxygen;             /* DS:B14C */
    uint16_t level_result;       /* DS:457C */
    uint16_t result_ticks;       /* DS:4578 */
} SrCellEffectState;

/* Exact road-cell low-nibble effects reconstructed from 1000:1A9C. */
void sr_apply_cell_effect(
    SrCellEffectState *state,
    uint16_t cell,
    void (*play_sound)(void *context, unsigned effect),
    void *sound_context);
void sr_apply_cell_effect_with_limit(
    SrCellEffectState *state,
    uint16_t cell,
    int32_t maximum_speed,
    void (*play_sound)(void *context, unsigned effect),
    void *sound_context);

#endif
