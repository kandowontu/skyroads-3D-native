#include "intro.h"

#include "palette.h"

#include <string.h>

enum {
    SR_INTRO_FADE_IN_TICKS = 0x24,
    SR_INTRO_SAMPLE_WAIT_TICKS = 0x18,
    SR_INTRO_ANIMATION_WAIT_TICKS = 0x25,
    SR_INTRO_ANIMATION_GROUP_TICKS = 2,
    SR_INTRO_ANIMATION_GROUPS = 100,
    SR_INTRO_LOGO_WAIT_TICKS = 0x48,
    SR_INTRO_LOGO_SLIDE_TICKS = 0x12,
    SR_INTRO_LOGO_TO_WHITE_TICKS = 5,
    SR_INTRO_LOGO_WHITE_HOLD_TICKS = 9,
    SR_INTRO_LOGO_FROM_WHITE_TICKS = 0x46,
    SR_INTRO_CARD_FIRST = 2,
    SR_INTRO_CARD_LAST = 6,
    SR_INTRO_CARD_FADE_TICKS = 0x32,
    SR_INTRO_CARD_HOLD_TICKS = 0x32,
    SR_INTRO_FINAL_FADE_TICKS = 0x24
};

static SrIntroStep step_from(const SrIntroSequence *sequence) {
    SrIntroStep step;
    memset(&step, 0, sizeof(step));
    step.phase = sequence->phase;
    step.phase_ticks = sequence->phase_ticks;
    step.animation_group = sequence->animation_group;
    step.card_index = sequence->card_index;
    return step;
}

void sr_intro_sequence_init(SrIntroSequence *sequence) {
    if (sequence == 0) return;
    memset(sequence, 0, sizeof(*sequence));
    sequence->phase = SR_INTRO_FADE_IN;
    sequence->card_index = SR_INTRO_CARD_FIRST;
}

SrIntroStep sr_intro_sequence_tick(SrIntroSequence *sequence, int any_key) {
    SrIntroStep step;
    if (sequence == 0) {
        memset(&step, 0, sizeof(step));
        step.phase = SR_INTRO_FINISHED;
        return step;
    }
    step = step_from(sequence);
    if (sequence->phase == SR_INTRO_FINISHED) return step;
    if (any_key) {
        sequence->aborted = 1;
        sequence->phase = SR_INTRO_FINISHED;
        sequence->phase_ticks = 0;
        step = step_from(sequence);
        step.events = SR_INTRO_EVENT_ABORTED | SR_INTRO_EVENT_FINISHED;
        return step;
    }

    ++sequence->phase_ticks;
    switch (sequence->phase) {
    case SR_INTRO_FADE_IN:
        step.palette_percent = sr_palette_transition_percent(
            sequence->phase_ticks, SR_INTRO_FADE_IN_TICKS);
        if (sequence->phase_ticks >= SR_INTRO_FADE_IN_TICKS) {
            sequence->phase = SR_INTRO_WAIT_FOR_SAMPLE;
            sequence->phase_ticks = 0;
        }
        break;

    case SR_INTRO_WAIT_FOR_SAMPLE:
        if (sequence->phase_ticks >= SR_INTRO_SAMPLE_WAIT_TICKS) {
            step.events |= SR_INTRO_EVENT_START_SAMPLE;
            sequence->phase = SR_INTRO_WAIT_FOR_ANIMATION;
            sequence->phase_ticks = 0;
        }
        break;

    case SR_INTRO_WAIT_FOR_ANIMATION:
        if (sequence->phase_ticks >= SR_INTRO_ANIMATION_WAIT_TICKS) {
            sequence->phase = SR_INTRO_ANIMATION;
            sequence->phase_ticks = 0;
        }
        break;

    case SR_INTRO_ANIMATION:
        if (sequence->phase_ticks >= SR_INTRO_ANIMATION_GROUP_TICKS) {
            step.events |= SR_INTRO_EVENT_DRAW_ANIMATION_GROUP;
            step.animation_group = sequence->animation_group;
            ++sequence->animation_group;
            sequence->phase_ticks = 0;
            if (sequence->animation_group >= SR_INTRO_ANIMATION_GROUPS) {
                sequence->phase = SR_INTRO_LOGO_WAIT;
            }
        }
        break;

    case SR_INTRO_LOGO_WAIT:
        if (sequence->phase_ticks >= SR_INTRO_LOGO_WAIT_TICKS) {
            sequence->phase = SR_INTRO_LOGO_SLIDE;
            sequence->phase_ticks = 0;
            step.events |= SR_INTRO_EVENT_DRAW_LOGO;
            step.logo_offset = 0x13f;
        }
        break;

    case SR_INTRO_LOGO_SLIDE:
        step.events |= SR_INTRO_EVENT_DRAW_LOGO;
        step.logo_offset = (uint16_t)(
            0x13fu - ((uint32_t)sequence->phase_ticks * 0x13fu /
                SR_INTRO_LOGO_SLIDE_TICKS));
        if (sequence->phase_ticks >= SR_INTRO_LOGO_SLIDE_TICKS) {
            step.logo_offset = 0;
            sequence->phase = SR_INTRO_LOGO_TO_WHITE;
            sequence->phase_ticks = 0;
        }
        break;

    case SR_INTRO_LOGO_TO_WHITE:
        step.palette_percent = sr_palette_transition_percent(
            sequence->phase_ticks, SR_INTRO_LOGO_TO_WHITE_TICKS);
        if (sequence->phase_ticks >= SR_INTRO_LOGO_TO_WHITE_TICKS) {
            sequence->phase = SR_INTRO_LOGO_WHITE_HOLD;
            sequence->phase_ticks = 0;
        }
        break;

    case SR_INTRO_LOGO_WHITE_HOLD:
        if (sequence->phase_ticks >= SR_INTRO_LOGO_WHITE_HOLD_TICKS) {
            sequence->phase = SR_INTRO_LOGO_FROM_WHITE;
            sequence->phase_ticks = 0;
        }
        break;

    case SR_INTRO_LOGO_FROM_WHITE:
        step.palette_percent = sr_palette_transition_percent(
            sequence->phase_ticks, SR_INTRO_LOGO_FROM_WHITE_TICKS);
        if (sequence->phase_ticks >= SR_INTRO_LOGO_FROM_WHITE_TICKS) {
            sequence->phase = SR_INTRO_CARD_FADE_IN;
            sequence->phase_ticks = 0;
            step.events |= SR_INTRO_EVENT_BEGIN_CARD;
            step.card_index = sequence->card_index;
        }
        break;

    case SR_INTRO_CARD_FADE_IN:
        step.palette_percent = sr_palette_transition_percent(
            sequence->phase_ticks, SR_INTRO_CARD_FADE_TICKS);
        if (sequence->phase_ticks >= SR_INTRO_CARD_FADE_TICKS) {
            sequence->phase = SR_INTRO_CARD_HOLD;
            sequence->phase_ticks = 0;
        }
        break;

    case SR_INTRO_CARD_HOLD:
        if (sequence->phase_ticks >= SR_INTRO_CARD_HOLD_TICKS) {
            sequence->phase = SR_INTRO_CARD_FADE_OUT;
            sequence->phase_ticks = 0;
        }
        break;

    case SR_INTRO_CARD_FADE_OUT:
        step.palette_percent = sr_palette_transition_percent(
            sequence->phase_ticks, SR_INTRO_CARD_FADE_TICKS);
        if (sequence->phase_ticks >= SR_INTRO_CARD_FADE_TICKS) {
            step.events |= SR_INTRO_EVENT_CLEAR_CARD;
            sequence->phase_ticks = 0;
            if (sequence->card_index < SR_INTRO_CARD_LAST) {
                ++sequence->card_index;
                sequence->phase = SR_INTRO_CARD_FADE_IN;
                step.events |= SR_INTRO_EVENT_BEGIN_CARD;
                step.card_index = sequence->card_index;
            }
            else {
                sequence->phase = SR_INTRO_FINAL_FADE_OUT;
                step.events |= SR_INTRO_EVENT_BEGIN_FINAL_FADE;
            }
        }
        break;

    case SR_INTRO_FINAL_FADE_OUT:
        step.palette_percent = sr_palette_transition_percent(
            sequence->phase_ticks, SR_INTRO_FINAL_FADE_TICKS);
        if (sequence->phase_ticks >= SR_INTRO_FINAL_FADE_TICKS) {
            sequence->phase = SR_INTRO_FINISHED;
            sequence->phase_ticks = 0;
            step.events |= SR_INTRO_EVENT_FINISHED;
        }
        break;

    case SR_INTRO_FINISHED:
        break;
    }
    step.phase = sequence->phase;
    step.phase_ticks = sequence->phase_ticks;
    return step;
}
