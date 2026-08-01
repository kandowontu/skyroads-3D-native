#include "physics.h"

int16_t sr_gravity_step(uint16_t road_gravity) {
    return (int16_t)-(int32_t)((uint32_t)road_gravity * 0x1680u / 400u);
}

int32_t sr_adjust_forward_speed(int32_t speed, int16_t throttle) {
    return sr_adjust_forward_speed_with_limit(speed, throttle, 0x2aaa);
}

int32_t sr_adjust_forward_speed_with_limit(
    int32_t speed,
    int16_t throttle,
    int32_t maximum_speed) {
    if (maximum_speed <= 0) {
        maximum_speed = 0x2aaa;
    }
    speed += (int32_t)throttle * 0x4b;
    if (speed < 0) {
        return 0;
    }
    if (speed > maximum_speed) {
        return maximum_speed;
    }
    return speed;
}

int16_t sr_apply_airborne_gravity(
    uint16_t ship_height,
    int16_t vertical_velocity,
    int16_t gravity_step) {
    if (ship_height < 0x2800) {
        if (vertical_velocity > -0x6a) {
            vertical_velocity = -0x6a;
        }
    }
    else {
        vertical_velocity = (int16_t)(vertical_velocity + gravity_step);
    }
    return vertical_velocity;
}

uint16_t sr_consume_oxygen(uint16_t oxygen, uint16_t road_oxygen) {
    uint16_t divisor = (uint16_t)(road_oxygen * 0x24u);
    uint16_t consumed = divisor == 0 ? 0 : (uint16_t)(30000u / divisor);
    return consumed > oxygen ? 0 : (uint16_t)(oxygen - consumed);
}

uint16_t sr_consume_fuel(uint16_t fuel, uint16_t road_fuel, int32_t forward_speed) {
    uint32_t scale;
    uint32_t consumed;
    if (road_fuel == 0 || forward_speed <= 0) {
        return fuel;
    }
    scale = 30000u / road_fuel;
    consumed = (uint32_t)(((uint64_t)scale * (uint32_t)forward_speed) / 0x10000u);
    return consumed > fuel ? 0 : (uint16_t)(fuel - consumed);
}

void sr_apply_cell_effect(
    SrCellEffectState *state,
    uint16_t cell,
    void (*play_sound)(void *context, unsigned effect),
    void *sound_context) {
    sr_apply_cell_effect_with_limit(
        state, cell, 0x2aaa, play_sound, sound_context);
}

void sr_apply_cell_effect_with_limit(
    SrCellEffectState *state,
    uint16_t cell,
    int32_t maximum_speed,
    void (*play_sound)(void *context, unsigned effect),
    void *sound_context) {
    uint16_t kind = cell & 0x000fu;

    if (maximum_speed <= 0) {
        maximum_speed = 0x2aaa;
    }

    if (kind == 2) {
        if (state->result_ticks == 0) {
            state->forward_speed -= 0x12f;
        }
    }
    else if (kind == 9) {
        if (state->level_result == 0) {
            if ((state->fuel < 27000 || state->oxygen < 27000) && play_sound != 0) {
                play_sound(sound_context, 4);
            }
            state->fuel = 30000;
            state->oxygen = 30000;
        }
    }
    else if (kind == 10) {
        if (state->result_ticks == 0) {
            state->forward_speed += 0x12f;
        }
    }
    else if (kind == 12) {
        if (state->level_result == 0) {
            state->level_result = 2;
        }
        if (state->result_ticks == 0) {
            state->result_ticks = 1;
            if (play_sound != 0) {
                play_sound(sound_context, 0);
            }
        }
    }

    if (state->forward_speed < 0) {
        state->forward_speed = 0;
    }
    else if (state->forward_speed > maximum_speed) {
        state->forward_speed = maximum_speed;
    }
}
