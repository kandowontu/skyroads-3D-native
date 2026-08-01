#ifndef SKYROADS_RECOVERED_BIOS_FONT_H
#define SKYROADS_RECOVERED_BIOS_FONT_H

#include <stdint.h>

enum {
    SR_BIOS_TEXT_WIDTH = 320,
    SR_BIOS_TEXT_HEIGHT = 200
};

/*
 * VGA branch of skyroads.exe 1000:44A2/450A using the BIOS 8x8 bank requested
 * with INT 10h AX=1130h, BH=3 at startup.  Pixels whose font bits are zero are
 * left untouched.
 */
int sr_draw_bios_text_vga(
    uint8_t framebuffer[SR_BIOS_TEXT_WIDTH * SR_BIOS_TEXT_HEIGHT],
    uint16_t x,
    uint16_t y,
    const char *text,
    uint8_t color);

/* Native menu extension using the same IBM-compatible glyph bank. */
int sr_draw_bios_text_scaled_vga(
    uint8_t framebuffer[SR_BIOS_TEXT_WIDTH * SR_BIOS_TEXT_HEIGHT],
    uint16_t x,
    uint16_t y,
    const char *text,
    uint8_t color,
    uint16_t scale_x,
    uint16_t scale_y);

#endif
