#include "palette.h"

#include <stdint.h>

int main(void) {
    static const uint8_t from[] = { 0, 63, 50, 5, 17, 63 };
    static const uint8_t to[] = { 63, 0, 5, 50, 18, 0 };
    uint8_t output[sizeof(from)];

    if (sr_palette_transition_percent(0, 36) != 0 ||
        sr_palette_transition_percent(1, 36) != 2 ||
        sr_palette_transition_percent(18, 36) != 50 ||
        sr_palette_transition_percent(35, 36) != 97 ||
        sr_palette_transition_percent(36, 36) != 100 ||
        sr_palette_transition_percent(40, 36) != 100 ||
        sr_palette_transition_percent(0, 0) != 100) return 1;

    if (!sr_blend_palette_bytes(output, from, to, sizeof(from), 50)) return 1;
    /* IDIV truncates signed deltas toward zero at 1000:43F9. */
    if (output[0] != 31 || output[1] != 32 || output[2] != 28 ||
        output[3] != 27 || output[4] != 17 || output[5] != 32) return 1;
    if (!sr_blend_palette_bytes(output, from, to, sizeof(from), 100)) return 1;
    for (unsigned index = 0; index < sizeof(from); ++index) {
        if (output[index] != to[index]) return 1;
    }
    return 0;
}
