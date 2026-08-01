#ifndef SKYROADS_RECOVERED_GRAPHICS_ARCHIVE_H
#define SKYROADS_RECOVERED_GRAPHICS_ARCHIVE_H

#include <stddef.h>
#include <stdint.h>

typedef enum SrGraphicsArchiveError {
    SR_GRAPHICS_ARCHIVE_OK = 0,
    SR_GRAPHICS_ARCHIVE_TRUNCATED,
    SR_GRAPHICS_ARCHIVE_BAD_CMAP,
    SR_GRAPHICS_ARCHIVE_BAD_PICTURE,
    SR_GRAPHICS_ARCHIVE_OUT_OF_MEMORY,
    SR_GRAPHICS_ARCHIVE_DECOMPRESSION_FAILED
} SrGraphicsArchiveError;

typedef struct SrPicture {
    uint16_t screen_offset;
    uint16_t height;
    uint16_t width;
    uint8_t *pixels;
    size_t pixel_count;
} SrPicture;

typedef struct SrGraphicsArchive {
    uint8_t palette_count;
    uint8_t palette[256 * 3];
    uint16_t ega_color_pairs[256];
    SrPicture *pictures;
    size_t picture_count;
    size_t input_consumed;
} SrGraphicsArchive;

SrGraphicsArchiveError sr_load_vga_picture(
    const uint8_t *bytes,
    size_t size,
    uint8_t palette_base,
    SrPicture *picture,
    size_t *input_consumed);

void sr_free_picture(SrPicture *picture);

/* Memory translation of CMAP loader 3F75 and VGA PICT loader 4068/4036.
   palette_base is added to each nonzero decompressed picture byte. */
SrGraphicsArchiveError sr_load_vga_graphics_archive(
    const uint8_t *bytes,
    size_t size,
    uint8_t palette_base,
    SrGraphicsArchive *archive);

void sr_free_graphics_archive(SrGraphicsArchive *archive);

#endif
