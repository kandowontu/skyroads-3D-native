#include "intro.h"

#include <stdio.h>

int main(void) {
    SrIntroSequence sequence;
    SrIntroStep step;
    unsigned tick;
    unsigned sample_tick = 0;
    unsigned first_group_tick = 0;
    unsigned last_group_tick = 0;
    unsigned first_logo_tick = 0;
    unsigned first_card_tick = 0;
    unsigned final_tick = 0;
    unsigned groups = 0;
    unsigned cards = 0;

    sr_intro_sequence_init(&sequence);
    for (tick = 1; tick < 2000; ++tick) {
        step = sr_intro_sequence_tick(&sequence, 0);
        if ((step.events & SR_INTRO_EVENT_START_SAMPLE) != 0) sample_tick = tick;
        if ((step.events & SR_INTRO_EVENT_DRAW_ANIMATION_GROUP) != 0) {
            if (step.animation_group != groups) return 1;
            if (groups == 0) first_group_tick = tick;
            last_group_tick = tick;
            ++groups;
        }
        if ((step.events & SR_INTRO_EVENT_DRAW_LOGO) != 0 && first_logo_tick == 0) {
            first_logo_tick = tick;
            if (step.logo_offset != 0x13f) return 1;
        }
        if ((step.events & SR_INTRO_EVENT_BEGIN_CARD) != 0) {
            if (step.card_index != cards + 2u) return 1;
            if (cards == 0) first_card_tick = tick;
            ++cards;
        }
        if ((step.events & SR_INTRO_EVENT_FINISHED) != 0) {
            final_tick = tick;
            break;
        }
    }
    printf(
        "intro timeline: sample=%u groups=%u first=%u last=%u logo=%u "
        "cards=%u first_card=%u final=%u\n",
        sample_tick, groups, first_group_tick, last_group_tick,
        first_logo_tick, cards, first_card_tick, final_tick);
    if (sample_tick != 60 || groups != 100 || first_group_tick != 99 ||
        last_group_tick != 297 || first_logo_tick != 369 || cards != 5 ||
        first_card_tick != 471 || final_tick != 1257) return 1;

    sr_intro_sequence_init(&sequence);
    step = sr_intro_sequence_tick(&sequence, 1);
    if ((step.events & (SR_INTRO_EVENT_ABORTED | SR_INTRO_EVENT_FINISHED)) !=
            (SR_INTRO_EVENT_ABORTED | SR_INTRO_EVENT_FINISHED) ||
        sequence.phase != SR_INTRO_FINISHED || sequence.aborted == 0) return 1;
    return 0;
}
