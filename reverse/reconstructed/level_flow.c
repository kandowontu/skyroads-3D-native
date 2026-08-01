#include "level_flow.h"

#include "palette.h"

#include <string.h>

enum {
    SR_RUN_LEVEL_FADE_TICKS = 0x24,
    SR_RUN_LEVEL_COMPLETION_TICKS = 0x1b
};

static SrRunLevelStep make_step(const SrRunLevelState *state) {
    SrRunLevelStep step;
    memset(&step, 0, sizeof(step));
    step.phase = state->phase;
    step.result = state->result;
    step.final_unfinished_level = state->final_unfinished_level;
    return step;
}

void sr_run_level_init(SrRunLevelState *state, int final_unfinished_level) {
    if (state == 0) return;
    memset(state, 0, sizeof(*state));
    state->phase = SR_RUN_LEVEL_FADE_IN;
    state->final_unfinished_level = (uint16_t)(final_unfinished_level != 0);
}

SrRunLevelStep sr_run_level_finish_gameplay(
    SrRunLevelState *state,
    uint16_t result) {
    SrRunLevelStep step;
    if (state == 0) {
        memset(&step, 0, sizeof(step));
        return step;
    }
    state->result = result;
    state->phase_ticks = 0;
    if (result == 0) {
        state->phase = SR_RUN_LEVEL_COMPLETION_HOLD;
        step = make_step(state);
        step.events = SR_RUN_LEVEL_EVENT_DRAW_COMPLETION;
    }
    else {
        state->phase = SR_RUN_LEVEL_FADE_OUT;
        step = make_step(state);
    }
    return step;
}

SrRunLevelStep sr_run_level_tick(SrRunLevelState *state) {
    SrRunLevelStep step;
    if (state == 0) {
        memset(&step, 0, sizeof(step));
        return step;
    }
    step = make_step(state);
    if (state->phase == SR_RUN_LEVEL_GAMEPLAY ||
        state->phase == SR_RUN_LEVEL_FINISHED) return step;
    ++state->phase_ticks;
    if (state->phase == SR_RUN_LEVEL_FADE_IN) {
        step.palette_percent = sr_palette_transition_percent(
            state->phase_ticks, SR_RUN_LEVEL_FADE_TICKS);
        if (state->phase_ticks >= SR_RUN_LEVEL_FADE_TICKS) {
            state->phase = SR_RUN_LEVEL_GAMEPLAY;
            state->phase_ticks = 0;
            step.events |= SR_RUN_LEVEL_EVENT_START_GAMEPLAY;
        }
    }
    else if (state->phase == SR_RUN_LEVEL_COMPLETION_HOLD) {
        if (state->phase_ticks >= SR_RUN_LEVEL_COMPLETION_TICKS) {
            state->phase = SR_RUN_LEVEL_FADE_OUT;
            state->phase_ticks = 0;
        }
    }
    else if (state->phase == SR_RUN_LEVEL_FADE_OUT) {
        step.palette_percent = sr_palette_transition_percent(
            state->phase_ticks, SR_RUN_LEVEL_FADE_TICKS);
        if (state->phase_ticks >= SR_RUN_LEVEL_FADE_TICKS) {
            state->phase = SR_RUN_LEVEL_FINISHED;
            state->phase_ticks = 0;
            step.events |= SR_RUN_LEVEL_EVENT_FINISHED;
        }
    }
    step.phase = state->phase;
    return step;
}
