#include "bios_font.h"

#include <stddef.h>

typedef struct SrBiosGlyph {
    unsigned char character;
    uint8_t rows[8];
} SrBiosGlyph;

/* Exact IBM-compatible BH=3 glyph rows used by run_level's two messages. */
static const SrBiosGlyph glyphs[] = {
    {' ', {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
    {'C', {0x3c,0x66,0xc0,0xc0,0xc0,0x66,0x3c,0x00}},
    {'E', {0xfe,0x62,0x68,0x78,0x68,0x62,0xfe,0x00}},
    {'R', {0xfc,0x66,0x66,0x7c,0x78,0x6c,0xe6,0x00}},
    {'T', {0xfc,0xb4,0x30,0x30,0x30,0x30,0x78,0x00}},
    {'a', {0x00,0x00,0x78,0x0c,0x7c,0xcc,0x76,0x00}},
    {'d', {0x1c,0x0c,0x0c,0x7c,0xcc,0xcc,0x76,0x00}},
    {'e', {0x00,0x00,0x78,0xcc,0xfc,0xc0,0x78,0x00}},
    {'h', {0xe0,0x60,0x6c,0x76,0x66,0x66,0xe6,0x00}},
    {'i', {0x30,0x00,0x70,0x30,0x30,0x30,0x78,0x00}},
    {'l', {0x70,0x30,0x30,0x30,0x30,0x30,0x78,0x00}},
    {'m', {0x00,0x00,0xec,0xfe,0xd6,0xc6,0xc6,0x00}},
    {'n', {0x00,0x00,0xf8,0xcc,0xcc,0xcc,0xcc,0x00}},
    {'o', {0x00,0x00,0x78,0xcc,0xcc,0xcc,0x78,0x00}},
    {'p', {0x00,0x00,0xdc,0x66,0x66,0x7c,0x60,0xf0}},
    {'r', {0x00,0x00,0xdc,0x76,0x66,0x60,0xf0,0x00}},
    {'t', {0x10,0x30,0x7c,0x30,0x30,0x34,0x18,0x00}}
};

static const uint8_t *find_glyph(unsigned char character) {
    size_t index;
    for (index = 0; index < sizeof(glyphs) / sizeof(glyphs[0]); ++index) {
        if (glyphs[index].character == character) return glyphs[index].rows;
    }
    return 0;
}

int sr_draw_bios_text_vga(
    uint8_t framebuffer[SR_BIOS_TEXT_WIDTH * SR_BIOS_TEXT_HEIGHT],
    uint16_t x,
    uint16_t y,
    const char *text,
    uint8_t color) {
    return sr_draw_bios_text_scaled_vga(
        framebuffer, x, y, text, color, 1u, 1u);
}

int sr_draw_bios_text_scaled_vga(
    uint8_t framebuffer[SR_BIOS_TEXT_WIDTH * SR_BIOS_TEXT_HEIGHT],
    uint16_t x,
    uint16_t y,
    const char *text,
    uint8_t color,
    uint16_t scale_x,
    uint16_t scale_y) {
    if (framebuffer == 0 || text == 0) return 0;
    if (scale_x == 0 || scale_y == 0) return 0;
    while (*text != '\0') {
        const uint8_t *glyph = find_glyph((unsigned char)*text);
        unsigned row;
        if (glyph == 0 || x + 8u * scale_x > SR_BIOS_TEXT_WIDTH ||
            y + 8u * scale_y > SR_BIOS_TEXT_HEIGHT) return 0;
        for (row = 0; row < 8u; ++row) {
            uint8_t bits = glyph[row];
            unsigned column;
            for (column = 0; column < 8u; ++column) {
                if ((bits & (uint8_t)(0x80u >> column)) != 0) {
                    unsigned repeat_y;
                    for (repeat_y = 0; repeat_y < scale_y; ++repeat_y) {
                        unsigned repeat_x;
                        for (repeat_x = 0; repeat_x < scale_x; ++repeat_x) {
                            framebuffer[
                                (size_t)(y + row * scale_y + repeat_y) *
                                    SR_BIOS_TEXT_WIDTH +
                                x + column * scale_x + repeat_x] = color;
                        }
                    }
                }
            }
        }
        x = (uint16_t)(x + 8u * scale_x);
        ++text;
    }
    return 1;
}
