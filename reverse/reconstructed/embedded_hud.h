#ifndef SKYROADS_RECOVERED_EMBEDDED_HUD_H
#define SKYROADS_RECOVERED_EMBEDDED_HUD_H

#include <stddef.h>
#include <stdint.h>

typedef struct SrEmbeddedHud {
    uint8_t digits[10][20];
    uint8_t jumpmaster[2][130];
    uint16_t decimal_divisors[4];
} SrEmbeddedHud;

/* Extracts DS:013C..030F from the exact MZ image used by the runtime HUD. */
int sr_load_embedded_hud_from_exe(
    const uint8_t *bytes,
    size_t size,
    SrEmbeddedHud *hud);

#endif
