#ifndef SKYROADS_RECOVERED_KEYBOARD_H
#define SKYROADS_RECOVERED_KEYBOARD_H

#include <stdint.h>

typedef struct SrKeyboardDrainState {
    uint16_t intro_abort_enabled; /* DS:AF42 */
    uint16_t intro_aborted;       /* DS:54AC */
} SrKeyboardDrainState;

typedef struct SrKeyboardHooks {
    void *context;
    int (*key_available)(void *context);
    uint16_t (*read_key)(void *context);
} SrKeyboardHooks;

/* Drains BIOS key events and latches the abort flag, skyroads.exe 1000:4137. */
unsigned sr_drain_keyboard_and_latch_intro_abort(
    SrKeyboardDrainState *state,
    const SrKeyboardHooks *hooks);

#endif
