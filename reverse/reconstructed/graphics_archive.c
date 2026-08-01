#include "graphics_archive.h"

#include <stdlib.h>
#include <string.h>

#include "lzs.h"

static uint16_t read_u16(const uint8_t *bytes) {
    return (uint16_t)(bytes[0] | ((uint16_t)bytes[1] << 8));
}

void sr_free_picture(SrPicture *picture) {
    if (picture == 0) return;
    free(picture->pixels);
    memset(picture, 0, sizeof(*picture));
}

SrGraphicsArchiveError sr_load_vga_picture(
    const uint8_t *bytes,
    size_t size,
    uint8_t palette_base,
    SrPicture *picture,
    size_t *input_consumed) {
    SrLzsResult decoded;
    size_t pixel;
    if (picture == 0 || input_consumed == 0) return SR_GRAPHICS_ARCHIVE_BAD_PICTURE;
    memset(picture, 0, sizeof(*picture));
    *input_consumed = 0;
    if (bytes == 0 || size < 10u) return SR_GRAPHICS_ARCHIVE_TRUNCATED;
    if (memcmp(bytes, "PICT", 4) != 0) return SR_GRAPHICS_ARCHIVE_BAD_PICTURE;
    picture->screen_offset = read_u16(bytes + 4u);
    picture->height = read_u16(bytes + 6u);
    picture->width = read_u16(bytes + 8u);
    picture->pixel_count = (size_t)picture->height * picture->width;
    if (picture->pixel_count == 0) return SR_GRAPHICS_ARCHIVE_BAD_PICTURE;
    picture->pixels = (uint8_t *)malloc(picture->pixel_count);
    if (picture->pixels == 0) return SR_GRAPHICS_ARCHIVE_OUT_OF_MEMORY;
    decoded = sr_lzs_decompress(
        bytes + 10u, size - 10u, picture->pixels, picture->pixel_count);
    if (decoded.error != SR_LZS_OK || decoded.output_written != picture->pixel_count ||
        decoded.input_consumed > size - 10u) {
        sr_free_picture(picture);
        return SR_GRAPHICS_ARCHIVE_DECOMPRESSION_FAILED;
    }
    for (pixel = 0; pixel < picture->pixel_count; ++pixel) {
        if (picture->pixels[pixel] != 0) {
            picture->pixels[pixel] =
                (uint8_t)(picture->pixels[pixel] + palette_base);
        }
    }
    *input_consumed = 10u + decoded.input_consumed;
    return SR_GRAPHICS_ARCHIVE_OK;
}

void sr_free_graphics_archive(SrGraphicsArchive *archive) {
    size_t index;
    if (archive == 0) return;
    for (index = 0; index < archive->picture_count; ++index) {
        free(archive->pictures[index].pixels);
    }
    free(archive->pictures);
    memset(archive, 0, sizeof(*archive));
}

static SrGraphicsArchiveError append_picture(
    SrGraphicsArchive *archive,
    SrPicture *picture) {
    SrPicture *pictures = (SrPicture *)realloc(
        archive->pictures, (archive->picture_count + 1u) * sizeof(*pictures));
    if (pictures == 0) return SR_GRAPHICS_ARCHIVE_OUT_OF_MEMORY;
    archive->pictures = pictures;
    archive->pictures[archive->picture_count++] = *picture;
    return SR_GRAPHICS_ARCHIVE_OK;
}

SrGraphicsArchiveError sr_load_vga_graphics_archive(
    const uint8_t *bytes,
    size_t size,
    uint8_t palette_base,
    SrGraphicsArchive *archive) {
    size_t offset;
    size_t palette_bytes;
    size_t mapping_bytes;
    size_t index;

    if (archive == 0) return SR_GRAPHICS_ARCHIVE_BAD_CMAP;
    memset(archive, 0, sizeof(*archive));
    if (bytes == 0 || size < 5u) return SR_GRAPHICS_ARCHIVE_TRUNCATED;
    if (memcmp(bytes, "CMAP", 4) != 0) return SR_GRAPHICS_ARCHIVE_BAD_CMAP;

    archive->palette_count = bytes[4];
    palette_bytes = (size_t)archive->palette_count * 3u;
    mapping_bytes = (size_t)archive->palette_count * 2u;
    if (palette_bytes > size - 5u || mapping_bytes > size - 5u - palette_bytes) {
        sr_free_graphics_archive(archive);
        return SR_GRAPHICS_ARCHIVE_TRUNCATED;
    }
    memcpy(archive->palette, bytes + 5u, palette_bytes);
    offset = 5u + palette_bytes;
    for (index = 0; index < archive->palette_count; ++index) {
        archive->ega_color_pairs[index] = read_u16(bytes + offset + index * 2u);
    }
    offset += mapping_bytes;

    while (offset < size) {
        SrPicture picture;
        SrGraphicsArchiveError appended;
        SrGraphicsArchiveError decoded;
        size_t picture_consumed;
        memset(&picture, 0, sizeof(picture));
        /* The stream-oriented original reads only the PICT records requested
           by each caller.  Some files concatenate another CMAP sequence. */
        if (size - offset < 10u || memcmp(bytes + offset, "PICT", 4) != 0) break;
        decoded = sr_load_vga_picture(
            bytes + offset, size - offset, palette_base,
            &picture, &picture_consumed);
        if (decoded != SR_GRAPHICS_ARCHIVE_OK) {
            sr_free_graphics_archive(archive);
            return decoded;
        }
        appended = append_picture(archive, &picture);
        if (appended != SR_GRAPHICS_ARCHIVE_OK) {
            free(picture.pixels);
            sr_free_graphics_archive(archive);
            return appended;
        }
        offset += picture_consumed;
    }
    archive->input_consumed = offset;
    return archive->picture_count == 0
        ? SR_GRAPHICS_ARCHIVE_BAD_PICTURE : SR_GRAPHICS_ARCHIVE_OK;
}
