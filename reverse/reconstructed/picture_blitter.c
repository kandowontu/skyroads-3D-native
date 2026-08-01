#include "picture_blitter.h"

#include <string.h>

int sr_compose_picture_row(
    const SrPictureBlitter *blitter,
    size_t destination_offset,
    const uint8_t *source,
    size_t row_width,
    size_t left_reference_margin,
    size_t right_reference_margin) {
    size_t middle_width;
    size_t index;
    uint8_t *destination;
    if (blitter == 0 || blitter->destination == 0 || source == 0 ||
        left_reference_margin > row_width ||
        right_reference_margin > row_width - left_reference_margin ||
        destination_offset > blitter->destination_size ||
        row_width > blitter->destination_size - destination_offset) return 0;
    if ((left_reference_margin != 0 || right_reference_margin != 0 ||
         blitter->restore_below > blitter->transparent_below) &&
        blitter->reference == 0) return 0;
    destination = blitter->destination + destination_offset;
    middle_width = row_width - left_reference_margin - right_reference_margin;
    if (left_reference_margin != 0) {
        memcpy(destination, blitter->reference + destination_offset,
            left_reference_margin);
    }
    destination += left_reference_margin;
    for (index = 0; index < middle_width; ++index) {
        uint8_t pixel = source[index];
        size_t absolute = destination_offset + left_reference_margin + index;
        if (pixel < blitter->transparent_below) continue;
        if (pixel < blitter->restore_below) pixel = blitter->reference[absolute];
        destination[index] = pixel;
    }
    if (right_reference_margin != 0) {
        size_t absolute = destination_offset + row_width - right_reference_margin;
        memcpy(blitter->destination + absolute, blitter->reference + absolute,
            right_reference_margin);
    }
    return 1;
}

int sr_draw_picture_composited(
    const SrPictureBlitter *blitter,
    const SrPicture *picture) {
    size_t row;
    if (blitter == 0 || picture == 0 || picture->pixels == 0) return 0;
    for (row = 0; row < picture->height; ++row) {
        size_t destination_offset = (size_t)picture->screen_offset +
            row * SR_PICTURE_SCREEN_WIDTH;
        if (!sr_compose_picture_row(
                blitter, destination_offset,
                picture->pixels + row * picture->width,
                picture->width, 0, 0)) return 0;
    }
    return 1;
}

int sr_copy_picture_vga(
    uint8_t destination[SR_PICTURE_SCREEN_SIZE],
    const SrPicture *picture) {
    size_t row;
    if (destination == 0 || picture == 0 || picture->pixels == 0) return 0;
    for (row = 0; row < picture->height; ++row) {
        size_t offset = (size_t)picture->screen_offset + row * SR_PICTURE_SCREEN_WIDTH;
        if (offset > SR_PICTURE_SCREEN_SIZE ||
            picture->width > SR_PICTURE_SCREEN_SIZE - offset) return 0;
        memcpy(destination + offset,
            picture->pixels + row * picture->width, picture->width);
    }
    return 1;
}
