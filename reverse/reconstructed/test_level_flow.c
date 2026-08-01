#include "bios_font.h"
#include "level_flow.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint64_t hash_bytes(const uint8_t *bytes, size_t size) {
    uint64_t hash = UINT64_C(1469598103934665603);
    size_t index;
    for (index = 0; index < size; ++index) {
        hash ^= bytes[index];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

int main(void) {
    SrRunLevelState state;
    SrRunLevelStep step;
    uint8_t framebuffer[SR_BIOS_TEXT_WIDTH * SR_BIOS_TEXT_HEIGHT];
    unsigned ticks;

    sr_run_level_init(&state, 0);
    for (ticks = 1; ticks <= 36; ++ticks) step = sr_run_level_tick(&state);
    if ((step.events & SR_RUN_LEVEL_EVENT_START_GAMEPLAY) == 0 ||
        state.phase != SR_RUN_LEVEL_GAMEPLAY || step.palette_percent != 100) return 1;
    step = sr_run_level_finish_gameplay(&state, 0);
    if ((step.events & SR_RUN_LEVEL_EVENT_DRAW_COMPLETION) == 0 ||
        state.phase != SR_RUN_LEVEL_COMPLETION_HOLD) return 1;
    for (ticks = 0; ticks < 27; ++ticks) sr_run_level_tick(&state);
    if (state.phase != SR_RUN_LEVEL_FADE_OUT) return 1;
    for (ticks = 1; ticks <= 36; ++ticks) step = sr_run_level_tick(&state);
    if ((step.events & SR_RUN_LEVEL_EVENT_FINISHED) == 0 ||
        state.phase != SR_RUN_LEVEL_FINISHED || ticks != 37) return 1;

    sr_run_level_init(&state, 1);
    for (ticks = 0; ticks < 36; ++ticks) sr_run_level_tick(&state);
    step = sr_run_level_finish_gameplay(&state, 3);
    if (step.events != 0 || state.phase != SR_RUN_LEVEL_FADE_OUT) return 1;

    memset(framebuffer, 0, sizeof(framebuffer));
    if (!sr_draw_bios_text_vga(framebuffer, 0x68, 0x50, "Road Completed", 99)) return 1;
    printf("completion text hash=%016llx\n",
        (unsigned long long)hash_bytes(framebuffer, sizeof(framebuffer)));
    if (hash_bytes(framebuffer, sizeof(framebuffer)) !=
        UINT64_C(0xf6eb18988cebbf7c)) return 1;
    return 0;
}
