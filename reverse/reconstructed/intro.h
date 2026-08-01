#ifndef SKYROADS_RECOVERED_INTRO_H
#define SKYROADS_RECOVERED_INTRO_H

#include <stdint.h>

typedef enum SrIntroPhase {
    SR_INTRO_FADE_IN = 0,
    SR_INTRO_WAIT_FOR_SAMPLE,
    SR_INTRO_WAIT_FOR_ANIMATION,
    SR_INTRO_ANIMATION,
    SR_INTRO_LOGO_WAIT,
    SR_INTRO_LOGO_SLIDE,
    SR_INTRO_LOGO_TO_WHITE,
    SR_INTRO_LOGO_WHITE_HOLD,
    SR_INTRO_LOGO_FROM_WHITE,
    SR_INTRO_CARD_FADE_IN,
    SR_INTRO_CARD_HOLD,
    SR_INTRO_CARD_FADE_OUT,
    SR_INTRO_FINAL_FADE_OUT,
    SR_INTRO_FINISHED
} SrIntroPhase;

enum SrIntroEventFlag {
    SR_INTRO_EVENT_NONE = 0,
    SR_INTRO_EVENT_START_SAMPLE = 1 << 0,
    SR_INTRO_EVENT_DRAW_ANIMATION_GROUP = 1 << 1,
    SR_INTRO_EVENT_DRAW_LOGO = 1 << 2,
    SR_INTRO_EVENT_BEGIN_CARD = 1 << 3,
    SR_INTRO_EVENT_CLEAR_CARD = 1 << 4,
    SR_INTRO_EVENT_BEGIN_FINAL_FADE = 1 << 5,
    SR_INTRO_EVENT_FINISHED = 1 << 6,
    SR_INTRO_EVENT_ABORTED = 1 << 7
};

typedef struct SrIntroSequence {
    SrIntroPhase phase;
    uint16_t phase_ticks;
    uint16_t animation_group;
    uint16_t card_index;
    uint16_t aborted;
} SrIntroSequence;

typedef struct SrIntroStep {
    uint16_t events;
    SrIntroPhase phase;
    uint16_t phase_ticks;
    uint16_t palette_percent;
    uint16_t animation_group;
    uint16_t card_index;
    uint16_t logo_offset;
} SrIntroStep;

/* Initial state for the VGA branch of skyroads.exe 1000:4575. */
void sr_intro_sequence_init(SrIntroSequence *sequence);

/*
 * Advances one original 36 Hz game tick.  The returned events are the drawing,
 * palette, and sample operations reached by the blocking DOS routine during
 * that tick.  any_key models the keyboard poll at 1000:4137.
 */
SrIntroStep sr_intro_sequence_tick(SrIntroSequence *sequence, int any_key);

#endif
