#include "gameplay.h"

#include <string.h>

#include "motion.h"
#include "physics.h"

static const uint16_t road_shape_heights[6] = {
    0x2800, 0x3200, 0x3200, 0x3200, 0x3c00, 0x3c00
};

static void play_sound(const SrGameplayHooks *hooks, unsigned effect) {
    if (hooks != 0 && hooks->play_sound != 0) {
        hooks->play_sound(hooks->context, effect);
    }
}

static int sound_is_active(const SrGameplayHooks *hooks) {
    return hooks != 0 && hooks->sound_is_active != 0 &&
        hooks->sound_is_active(hooks->context);
}

static int16_t bounce_velocity(int16_t velocity) {
    int16_t product = (int16_t)((int32_t)velocity * 5);
    return (int16_t)((int16_t)-product / 10);
}

static uint16_t absolute_velocity(int16_t velocity) {
    if (velocity < 0) {
        return (uint16_t)(-(uint16_t)velocity);
    }
    return (uint16_t)velocity;
}

static int32_t effective_speed_limit(const SrGameplayConfig *config) {
    return config->forward_speed_limit > 0
        ? config->forward_speed_limit : 0x2aaa;
}

static void clamp_speed(
    SrGameplayState *state,
    const SrGameplayConfig *config) {
    if (state->forward_speed < 0) {
        state->forward_speed = 0;
    }
    else if (state->forward_speed > effective_speed_limit(config)) {
        state->forward_speed = effective_speed_limit(config);
    }
}

static void apply_cell_effect(
    SrGameplayState *state,
    uint16_t cell,
    const SrGameplayConfig *config,
    const SrGameplayHooks *hooks) {
    SrCellEffectState effect_state;

    effect_state.forward_speed = state->forward_speed;
    effect_state.fuel = state->fuel;
    effect_state.oxygen = state->oxygen;
    effect_state.level_result = state->level_result;
    effect_state.result_ticks = state->result_ticks;
    sr_apply_cell_effect_with_limit(
        &effect_state,
        cell,
        effective_speed_limit(config),
        hooks != 0 ? hooks->play_sound : 0,
        hooks != 0 ? hooks->context : 0);
    state->forward_speed = effect_state.forward_speed;
    state->fuel = effect_state.fuel;
    state->oxygen = effect_state.oxygen;
    state->level_result = effect_state.level_result;
    state->result_ticks = effect_state.result_ticks;
}

static void resolve_motion(
    const uint16_t *cells,
    size_t row_count,
    const SrGameplayControls *controls,
    const SrGameplayConfig *config,
    SrGameplayState *state) {
    SrMotionState motion;

    motion.position = state->position;
    motion.forward_speed = state->forward_speed;
    motion.lateral_velocity = state->lateral_velocity;
    motion.vertical_velocity = state->vertical_velocity;
    motion.surface_impulse = state->surface_impulse;
    motion.gravity_step = state->gravity_step;
    motion.throttle = controls->throttle;
    motion.collision_speed_correction = state->collision_speed_correction;
    motion.collision_adjusted = state->collision_adjusted;
    motion.forward_speed_limit = effective_speed_limit(config);
    sr_resolve_collision_motion(cells, row_count, &motion);
    state->forward_speed = motion.forward_speed;
    state->lateral_velocity = motion.lateral_velocity;
    state->collision_speed_correction = motion.collision_speed_correction;
    state->collision_adjusted = motion.collision_adjusted;
}

void sr_gameplay_init(SrGameplayState *state, const SrGameplayConfig *config) {
    memset(state, 0, sizeof(*state));
    state->position.distance = 0x00030000u;
    state->position.horizontal_position = 0x8000u;
    state->position.height = 0x2800u;
    state->gravity_step = sr_gravity_step(config->road_gravity);
    state->fuel = 30000;
    state->oxygen = 30000;
    state->process_surface_cell = 1;

    /* BP-1C is not initialized by the DOS binary. 0x2800 is the equivalent
       deterministic value for the first tick because velocity and impulse are zero. */
    state->previous_target_height = 0x2800u;
}

int sr_gameplay_result_ready(const SrGameplayState *state) {
    int result_can_return =
        state->result_ticks == 0 || state->result_ticks > 0x2au;
    int result_type_can_return =
        state->level_result == SR_LEVEL_CRASH ||
        state->level_result == SR_LEVEL_TERMINAL ||
        (state->level_result == SR_LEVEL_FELL && state->result_ticks != 0) ||
        state->result_delay_ticks >= 0x6cu;
    return result_can_return && result_type_can_return;
}

SrGameplayTickResult sr_gameplay_tick(
    const uint16_t *cells,
    size_t row_count,
    const SrGameplayConfig *config,
    const SrGameplayControls *controls,
    const SrGameplayHooks *hooks,
    SrGameplayState *state) {
    uint16_t raw_cell;
    uint16_t effect_cell;
    int current_cell_nonzero;
    SrShipPosition target;
    int16_t saved_surface_impulse;

    if (config->input_lock_active) {
        uint32_t elapsed = state->demo_elapsed_ticks++;
        if (elapsed >= 0x90u && state->level_result == 0 && state->result_ticks == 0) {
            state->level_result = SR_LEVEL_TERMINAL;
            state->result_ticks = 1;
            play_sound(hooks, 0);
        }
    }

    raw_cell = sr_road_cell_at_position(
        cells,
        row_count,
        state->position.distance,
        state->position.horizontal_position);
    current_cell_nonzero = raw_cell != 0;
    effect_cell = raw_cell;
    if (state->process_surface_cell) {
        if (state->position.height > 0x2800u) {
            uint16_t shape_index = effect_cell >> 8;
            if (shape_index < 6 &&
                state->position.height == road_shape_heights[shape_index]) {
                effect_cell >>= 4;
            }
            else {
                effect_cell = 0;
            }
        }
        apply_cell_effect(state, effect_cell, config, hooks);
        state->on_kind_8 = (effect_cell & 0x000fu) == 8;
        state->on_kind_2 = (effect_cell & 0x000fu) == 2;
    }
    else {
        state->on_kind_2 = 0;
    }

    if (config->road_length_rows != 0) {
        uint32_t finish_distance =
            ((uint32_t)config->road_length_rows << 16) - 0x8000u;
        if (state->position.distance >= finish_distance &&
            sr_test_finish_gate(
                cells,
                row_count,
                state->position.distance,
                state->position.horizontal_position,
                state->position.height) &&
            state->level_result == 0) {
            return SR_GAMEPLAY_TICK_FINISHED;
        }
    }

    if (state->position.height != state->previous_target_height) {
        if (state->surface_impulse == 0 || state->support_distance > 1) {
            uint16_t bounce_threshold =
                (uint16_t)((uint16_t)(config->road_gravity * 0x0104u) / 8u);
            if (absolute_velocity(state->vertical_velocity) >= bounce_threshold &&
                state->result_ticks == 0) {
                if (state->level_result == 0 && state->vertical_velocity < 0 &&
                    !sound_is_active(hooks)) {
                    play_sound(hooks, 1);
                }
                state->vertical_velocity = bounce_velocity(state->vertical_velocity);
            }
            else {
                state->vertical_velocity = 0;
            }
        }
        else {
            state->vertical_velocity = 0;
        }
    }

    if (state->level_result == 0) {
        state->forward_speed = sr_adjust_forward_speed_with_limit(
            state->forward_speed, controls->throttle,
            effective_speed_limit(config));
        if (!state->on_kind_8 &&
            ((!state->jumping && current_cell_nonzero) ||
             (state->lateral_velocity == 0 && state->vertical_velocity > 0 &&
              (uint16_t)(state->position.height - state->jump_start_height) < 0x0f00u))) {
            state->lateral_velocity = (int16_t)(controls->steering * 0x1d);
        }
        if (!state->jumping && current_cell_nonzero && controls->jump != 0 &&
            config->road_gravity < 0x14u) {
            state->vertical_velocity = 0x0480;
            state->jumping = 1;
            state->jump_start_height = state->position.height;
        }
    }

    if (config->collision_resolution_enabled && state->jumping &&
        !state->prediction_already_run && state->position.height >= 0x3700u) {
        resolve_motion(cells, row_count, controls, config, state);
        state->prediction_already_run = 1;
    }

    if (state->result_ticks == 0) {
        state->vertical_velocity = sr_apply_airborne_gravity(
            state->position.height,
            state->vertical_velocity,
            state->gravity_step);
    }
    else {
        if (state->vertical_velocity < 0) {
            state->vertical_velocity = 0;
        }
        if (state->vertical_velocity < 0x47) {
            state->vertical_velocity = (int16_t)(state->vertical_velocity + 0x27);
        }
        else {
            state->vertical_velocity = 0x47;
        }
    }

    target.distance = state->position.distance + (uint32_t)state->forward_speed;
    saved_surface_impulse = state->surface_impulse;
    {
        int32_t lateral_base = state->forward_speed +
            (state->on_kind_2 ? 0 : 0x618);
        int32_t lateral_step = (int32_t)(
            ((int64_t)lateral_base * state->lateral_velocity) / 0x200);
        target.horizontal_position = (uint16_t)(
            state->position.horizontal_position + lateral_step + saved_surface_impulse);
    }
    target.height = (uint16_t)(state->position.height + state->vertical_velocity);
    state->previous_target_height = target.height;

    if ((state->position.horizontal_position < 0x2f80u &&
         target.horizontal_position > 0xd080u) ||
        (target.horizontal_position < 0x2f80u &&
         state->position.horizontal_position > 0xd080u)) {
        target.horizontal_position = state->position.horizontal_position;
    }

    sr_move_ship_with_collision(cells, row_count, &state->position, target);

    if (state->position.distance != target.distance &&
        state->position.horizontal_position == target.horizontal_position &&
        sr_test_ship_collision(
            cells,
            row_count,
            target.distance,
            state->position.horizontal_position,
            state->position.height)) {
        if (!sr_test_ship_collision(
                cells,
                row_count,
                target.distance,
                (uint16_t)(state->position.horizontal_position - 0x03a0u),
                state->position.height)) {
            state->position.horizontal_position =
                (uint16_t)(state->position.horizontal_position - 0x03a0u);
            target.distance = state->position.distance;
            play_sound(hooks, 2);
        }
        else if (!sr_test_ship_collision(
                     cells,
                     row_count,
                     target.distance,
                     (uint16_t)(state->position.horizontal_position + 0x03a0u),
                     state->position.height)) {
            state->position.horizontal_position =
                (uint16_t)(state->position.horizontal_position + 0x03a0u);
            target.distance = state->position.distance;
            play_sound(hooks, 2);
        }
    }

    if (state->position.distance != target.distance) {
        if (state->forward_speed < 0 || state->forward_speed < 0x0e38 ||
            state->result_ticks != 0) {
            uint32_t prior_distance =
                target.distance - (uint32_t)state->forward_speed;
            if (prior_distance > state->position.distance) {
                play_sound(hooks, 2);
            }
        }
        else {
            state->result_ticks = 1;
            play_sound(hooks, 0);
            if (state->level_result == 0) {
                state->level_result = SR_LEVEL_CRASH;
            }
        }
        state->forward_speed = 0;
    }

    if (state->position.horizontal_position != target.horizontal_position) {
        state->lateral_velocity = 0;
        if ((state->surface_impulse > 0 &&
             state->position.horizontal_position < target.horizontal_position) ||
            (state->surface_impulse < 0 &&
             target.horizontal_position < state->position.horizontal_position)) {
            state->surface_impulse = 0;
        }
        state->forward_speed -= 0x97;
        clamp_speed(state, config);
    }

    state->process_surface_cell = 0;
    if (state->position.height != target.height && state->vertical_velocity < 0) {
        int support_direction = 0;
        unsigned offset;

        state->collision_adjusted = 0;
        state->prediction_already_run = 0;
        state->jumping = 0;
        state->process_surface_cell = 1;
        state->forward_speed -= state->collision_speed_correction;
        clamp_speed(state, config);
        state->collision_speed_correction = 0;

        for (offset = 1; offset < 15; ++offset) {
            if (!sr_test_ship_collision(
                    cells,
                    row_count,
                    state->position.distance,
                    (uint16_t)(state->position.horizontal_position + offset * 0x80u),
                    (uint16_t)(state->position.height - 1u))) {
                support_direction = 1;
                state->support_distance = (uint16_t)offset;
                break;
            }
        }
        for (offset = 1; offset < 15; ++offset) {
            if (!sr_test_ship_collision(
                    cells,
                    row_count,
                    state->position.distance,
                    (uint16_t)(state->position.horizontal_position - offset * 0x80u),
                    (uint16_t)(state->position.height - 1u))) {
                --support_direction;
                state->support_distance = (uint16_t)offset;
                break;
            }
        }
        if (support_direction == 0) {
            state->surface_impulse = 0;
        }
        else {
            state->surface_impulse = (int16_t)(
                state->surface_impulse + support_direction * 0x11);
        }
    }

    if (state->position.height > 0x7fffu) {
        state->position.height = 0;
    }

    if (state->level_result == 0) {
        state->oxygen = sr_consume_oxygen(state->oxygen, config->road_oxygen);
        state->fuel = sr_consume_fuel(
            state->fuel, config->road_fuel, state->forward_speed);
    }

    if (state->level_result == 0) {
        if (state->position.height < 0x2800u) {
            state->level_result = SR_LEVEL_FELL;
        }
        if (state->fuel == 0) {
            state->level_result = SR_LEVEL_OUT_OF_FUEL;
        }
        if (state->oxygen == 0) {
            state->level_result = SR_LEVEL_OUT_OF_OXYGEN;
        }
    }
    else {
        state->result_delay_ticks = (uint16_t)(state->result_delay_ticks + 1u);
    }
    if (state->result_ticks != 0) {
        state->result_ticks = (uint16_t)(state->result_ticks + 1u);
    }
    return SR_GAMEPLAY_TICK_RUNNING;
}
