#ifndef SKYROADS_RECOVERED_PICTURE_BLITTER_H
#define SKYROADS_RECOVERED_PICTURE_BLITTER_H

#include <stddef.h>
#include <stdint.h>

#include "graphics_archive.h"

#define SR_PICTURE_SCREEN_WIDTH 320
#define SR_PICTURE_SCREEN_HEIGHT 200
#define SR_PICTURE_SCREEN_SIZE (SR_PICTURE_SCREEN_WIDTH * SR_PICTURE_SCREEN_HEIGHT)

typedef struct SrPictureBlitter {
    const uint8_t *reference;
    uint8_t *destination;
    size_t destination_size;
    uint8_t restore_below;
    uint8_t transparent_below;
} SrPictureBlitter;

/* Semantic form of configure_picture_blitter 4162 and the self-modifying
   compose_picture_row 4184. */
int sr_compose_picture_row(
    const SrPictureBlitter *blitter,
    size_t destination_offset,
    const uint8_t *source,
    size_t row_width,
    size_t left_reference_margin,
    size_t right_reference_margin);

/* VGA branch of draw_picture 41E5 and copy_picture_to_video 4293. */
int sr_draw_picture_composited(
    const SrPictureBlitter *blitter,
    const SrPicture *picture);
int sr_copy_picture_vga(
    uint8_t destination[SR_PICTURE_SCREEN_SIZE],
    const SrPicture *picture);

#endif
