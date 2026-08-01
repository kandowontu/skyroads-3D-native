#ifndef SKYROADS_RECOVERED_LEVEL_FLOW_H
#define SKYROADS_RECOVERED_LEVEL_FLOW_H

#include <stdint.h>

typedef enum SrRunLevelPhase {
    SR_RUN_LEVEL_FADE_IN = 0,
    SR_RUN_LEVEL_GAMEPLAY,
    SR_RUN_LEVEL_COMPLETION_HOLD,
    SR_RUN_LEVEL_FADE_OUT,
    SR_RUN_LEVEL_FINISHED
} SrRunLevelPhase;

enum SrRunLevelEventFlag {
    SR_RUN_LEVEL_EVENT_NONE = 0,
    SR_RUN_LEVEL_EVENT_START_GAMEPLAY = 1 << 0,
    SR_RUN_LEVEL_EVENT_DRAW_COMPLETION = 1 << 1,
    SR_RUN_LEVEL_EVENT_FINISHED = 1 << 2
};

typedef struct SrRunLevelState {
    SrRunLevelPhase phase;
    uint16_t phase_ticks;
    uint16_t result;
    uint16_t final_unfinished_level;
} SrRunLevelState;

typedef struct SrRunLevelStep {
    uint16_t events;
    SrRunLevelPhase phase;
    uint16_t palette_percent;
    uint16_t result;
    uint16_t final_unfinished_level;
} SrRunLevelStep;

/* Exact 36-tick entry fade in run_level at skyroads.exe 1000:2B21. */
void sr_run_level_init(SrRunLevelState *state, int final_unfinished_level);

/* Receives the exact return value of gameplay_loop. */
SrRunLevelStep sr_run_level_finish_gameplay(
    SrRunLevelState *state,
    uint16_t result);

/* Advances one original 36 Hz tick during run_level presentation. */
SrRunLevelStep sr_run_level_tick(SrRunLevelState *state);

#endif
