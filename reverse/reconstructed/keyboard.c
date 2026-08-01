#include "keyboard.h"

unsigned sr_drain_keyboard_and_latch_intro_abort(
    SrKeyboardDrainState *state,
    const SrKeyboardHooks *hooks) {
    unsigned drained = 0;
    if (state == 0 || hooks == 0 || hooks->key_available == 0 ||
        hooks->read_key == 0) return 0;
    while (hooks->key_available(hooks->context)) {
        (void)hooks->read_key(hooks->context);
        ++drained;
        if (state->intro_abort_enabled != 0) state->intro_aborted = 1;
    }
    return drained;
}
