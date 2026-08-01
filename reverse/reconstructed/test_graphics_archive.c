#include "graphics_archive.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint8_t *read_file(const char *path, size_t *size) {
    FILE *stream = fopen(path, "rb");
    long length;
    uint8_t *bytes;
    if (stream == 0) return 0;
    fseek(stream, 0, SEEK_END);
    length = ftell(stream);
    fseek(stream, 0, SEEK_SET);
    if (length < 0) { fclose(stream); return 0; }
    bytes = (uint8_t *)malloc((size_t)length);
    if (bytes == 0 || fread(bytes, 1, (size_t)length, stream) != (size_t)length) {
        fclose(stream); free(bytes); return 0;
    }
    fclose(stream); *size = (size_t)length; return bytes;
}

static void hash_byte(uint64_t *hash, uint8_t value) {
    *hash ^= value;
    *hash *= UINT64_C(1099511628211);
}

static void hash_u16(uint64_t *hash, uint16_t value) {
    hash_byte(hash, (uint8_t)value);
    hash_byte(hash, (uint8_t)(value >> 8));
}

int main(int argc, char **argv) {
    uint64_t hash = UINT64_C(1469598103934665603);
    size_t total_pictures = 0;
    size_t total_pixels = 0;
    int argument;
    if (argc != 18) return 1;
    for (argument = 1; argument < argc; ++argument) {
        uint8_t *bytes;
        size_t size;
        size_t index;
        SrGraphicsArchive archive;
        SrGraphicsArchiveError error;
        bytes = read_file(argv[argument], &size);
        if (bytes == 0) return 1;
        error = sr_load_vga_graphics_archive(bytes, size, 0, &archive);
        if (error != SR_GRAPHICS_ARCHIVE_OK) {
            fprintf(stderr, "%s: graphics loader error %d\n", argv[argument], (int)error);
            free(bytes);
            return 1;
        }
        free(bytes);
        hash_byte(&hash, archive.palette_count);
        for (index = 0; index < (size_t)archive.palette_count * 3u; ++index) {
            hash_byte(&hash, archive.palette[index]);
        }
        for (index = 0; index < archive.palette_count; ++index) {
            hash_u16(&hash, archive.ega_color_pairs[index]);
        }
        printf("%s: colors=%u pictures=%zu", argv[argument],
            (unsigned)archive.palette_count, archive.picture_count);
        for (index = 0; index < archive.picture_count; ++index) {
            const SrPicture *picture = &archive.pictures[index];
            size_t pixel;
            printf(" %ux%u@%04x", (unsigned)picture->width,
                (unsigned)picture->height, (unsigned)picture->screen_offset);
            hash_u16(&hash, picture->screen_offset);
            hash_u16(&hash, picture->height);
            hash_u16(&hash, picture->width);
            for (pixel = 0; pixel < picture->pixel_count; ++pixel) {
                hash_byte(&hash, picture->pixels[pixel]);
            }
            total_pixels += picture->pixel_count;
        }
        putchar('\n');
        total_pictures += archive.picture_count;
        sr_free_graphics_archive(&archive);
    }
    printf("graphics aggregate: pictures=%zu pixels=%zu hash=%016llx\n",
        total_pictures, total_pixels, (unsigned long long)hash);
    return total_pictures == 20 && total_pixels == 810108 &&
        hash == UINT64_C(0xc7f93307e3142e98) ? 0 : 1;
}
