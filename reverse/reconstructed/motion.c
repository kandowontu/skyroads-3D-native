#include "motion.h"

#include "physics.h"

static int prediction_cell_is_open_or_terminal(uint16_t cell) {
    uint16_t shape = cell & 0x0f00u;
    uint16_t kind;

    if (shape != 0) {
        if (shape == 0x0100u) {
            return 0;
        }
        kind = (cell >> 4) & 0x000fu;
    }
    else {
        kind = cell & 0x000fu;
    }
    return (kind == 0 && shape == 0) || kind == 0x0c;
}

static int32_t effective_speed_limit(const SrMotionState *state) {
    return state->forward_speed_limit > 0
        ? state->forward_speed_limit : 0x2aaa;
}

int sr_predict_collision_motion(
    const uint16_t *cells,
    size_t row_count,
    const SrMotionState *state) {
    uint32_t distance = state->position.distance;
    uint16_t horizontal = state->position.horizontal_position;
    uint16_t height = state->position.height;
    int32_t speed = state->forward_speed;
    int16_t lateral_velocity = state->lateral_velocity;
    int16_t vertical_velocity = state->vertical_velocity;
    uint32_t previous_distance;
    uint16_t previous_horizontal;

    do {
        int32_t lateral_step;
        previous_distance = distance;
        previous_horizontal = horizontal;
        vertical_velocity = (int16_t)(vertical_velocity + state->gravity_step);
        distance += (uint32_t)speed;
        lateral_step = (int32_t)(((int64_t)(speed + 0x618) * lateral_velocity) / 0x200);
        horizontal = (uint16_t)(horizontal + lateral_step + state->surface_impulse);
        if (horizontal < 0x2f80u || horizontal > 0xd080u) {
            return 0;
        }
        height = (uint16_t)(height + vertical_velocity);
        speed = sr_adjust_forward_speed_with_limit(
            speed, state->throttle, effective_speed_limit(state));
    } while (height > 0x2800u);

    if (prediction_cell_is_open_or_terminal(sr_road_cell_at_position(
            cells, row_count, previous_distance, previous_horizontal))) {
        return 0;
    }
    if (prediction_cell_is_open_or_terminal(sr_road_cell_at_position(
            cells, row_count, distance, horizontal))) {
        return 0;
    }
    return 1;
}

void sr_resolve_collision_motion(
    const uint16_t *cells,
    size_t row_count,
    SrMotionState *state) {
    int32_t original_speed;
    int16_t original_lateral;
    int attempt;

    if (sr_predict_collision_motion(cells, row_count, state)) {
        return;
    }

    original_speed = state->forward_speed;
    original_lateral = state->lateral_velocity;
    for (attempt = 1; attempt <= 6; ++attempt) {
        int16_t lateral_delta = (int16_t)((int32_t)original_lateral * attempt / 10);
        int32_t speed_delta;

        state->lateral_velocity = (int16_t)(original_lateral + lateral_delta);
        if (sr_predict_collision_motion(cells, row_count, state)) {
            break;
        }

        state->lateral_velocity = (int16_t)(original_lateral - lateral_delta);
        if (sr_predict_collision_motion(cells, row_count, state)) {
            break;
        }
        state->lateral_velocity = original_lateral;

        speed_delta = original_speed * attempt / 10;
        state->forward_speed = original_speed + speed_delta;
        if ((state->forward_speed < 0 ||
             state->forward_speed < effective_speed_limit(state)) &&
            sr_predict_collision_motion(cells, row_count, state)) {
            break;
        }

        state->forward_speed = original_speed - speed_delta;
        if (sr_predict_collision_motion(cells, row_count, state)) {
            break;
        }
        state->forward_speed = original_speed;
    }

    state->collision_speed_correction = state->forward_speed - original_speed;
    if (attempt <= 6) {
        state->collision_adjusted = 1;
    }
}
