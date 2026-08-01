#ifndef SKYROADS_RECOVERED_ANIMATION_ARCHIVE_H
#define SKYROADS_RECOVERED_ANIMATION_ARCHIVE_H

#include <stddef.h>
#include <stdint.h>

#include "graphics_archive.h"

typedef enum SrAnimationArchiveError {
    SR_ANIMATION_ARCHIVE_OK = 0,
    SR_ANIMATION_ARCHIVE_TRUNCATED,
    SR_ANIMATION_ARCHIVE_BAD_HEADER,
    SR_ANIMATION_ARCHIVE_BAD_CMAP,
    SR_ANIMATION_ARCHIVE_BAD_PICTURE,
    SR_ANIMATION_ARCHIVE_OUT_OF_MEMORY
} SrAnimationArchiveError;

typedef struct SrAnimationGroup {
    size_t first_frame;
    uint16_t frame_count;
} SrAnimationGroup;

typedef struct SrAnimationFrame {
    uint16_t group_index;
    SrPicture picture;
} SrAnimationFrame;

typedef struct SrAnimationArchive {
    uint8_t palette_count;
    uint8_t palette[256 * 3];
    uint16_t ega_color_pairs[256];
    SrAnimationGroup *groups;
    size_t group_count;
    SrAnimationFrame *frames;
    size_t frame_count;
} SrAnimationArchive;

/* Exact ANIM/CMAP/grouped-PICT stream consumed by the intro routine at 4575. */
SrAnimationArchiveError sr_load_animation_archive(
    const uint8_t *bytes,
    size_t size,
    SrAnimationArchive *archive);

void sr_free_animation_archive(SrAnimationArchive *archive);

#endif
