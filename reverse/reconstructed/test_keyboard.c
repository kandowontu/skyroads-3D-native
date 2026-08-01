#include "keyboard.h"

#include <stddef.h>
#include <stdint.h>

typedef struct FakeKeyboard {
    const uint16_t *keys;
    size_t count;
    size_t cursor;
} FakeKeyboard;

static int key_available(void *context) {
    FakeKeyboard *keyboard = (FakeKeyboard *)context;
    return keyboard->cursor < keyboard->count;
}

static uint16_t read_key(void *context) {
    FakeKeyboard *keyboard = (FakeKeyboard *)context;
    return keyboard->keys[keyboard->cursor++];
}

int main(void) {
    static const uint16_t keys[] = { 0x4800, 0x000d, 0x001b };
    FakeKeyboard keyboard = { keys, 3, 0 };
    SrKeyboardDrainState state = { 0, 0 };
    SrKeyboardHooks hooks = { &keyboard, key_available, read_key };

    if (sr_drain_keyboard_and_latch_intro_abort(&state, &hooks) != 3 ||
        keyboard.cursor != 3 || state.intro_aborted != 0) return 1;
    keyboard.cursor = 0;
    state.intro_abort_enabled = 1;
    if (sr_drain_keyboard_and_latch_intro_abort(&state, &hooks) != 3 ||
        state.intro_aborted != 1) return 1;
    return 0;
}
