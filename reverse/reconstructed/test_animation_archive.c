#include "animation_archive.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint8_t *read_file(const char *path, size_t *size) {
    FILE *stream = fopen(path, "rb");
    long length;
    uint8_t *bytes;
    if (stream == 0) return 0;
    fseek(stream, 0, SEEK_END); length = ftell(stream); fseek(stream, 0, SEEK_SET);
    if (length < 0) { fclose(stream); return 0; }
    bytes = (uint8_t *)malloc((size_t)length);
    if (bytes == 0 || fread(bytes, 1, (size_t)length, stream) != (size_t)length) {
        fclose(stream); free(bytes); return 0;
    }
    fclose(stream); *size = (size_t)length; return bytes;
}

static void hash_byte(uint64_t *hash, uint8_t value) {
    *hash ^= value; *hash *= UINT64_C(1099511628211);
}

static void hash_u16(uint64_t *hash, uint16_t value) {
    hash_byte(hash, (uint8_t)value); hash_byte(hash, (uint8_t)(value >> 8));
}

int main(int argc, char **argv) {
    uint8_t *bytes;
    size_t size;
    size_t index;
    size_t total_pixels = 0;
    uint64_t hash = UINT64_C(1469598103934665603);
    SrAnimationArchive archive;
    int success;
    if (argc != 2) return 1;
    bytes = read_file(argv[1], &size);
    if (bytes == 0 || sr_load_animation_archive(bytes, size, &archive) !=
            SR_ANIMATION_ARCHIVE_OK) {
        free(bytes); return 1;
    }
    free(bytes);
    hash_byte(&hash, archive.palette_count);
    for (index = 0; index < (size_t)archive.palette_count * 3u; ++index) {
        hash_byte(&hash, archive.palette[index]);
    }
    for (index = 0; index < archive.palette_count; ++index) {
        hash_u16(&hash, archive.ega_color_pairs[index]);
    }
    for (index = 0; index < archive.group_count; ++index) {
        hash_u16(&hash, archive.groups[index].frame_count);
    }
    for (index = 0; index < archive.frame_count; ++index) {
        const SrAnimationFrame *frame = &archive.frames[index];
        size_t pixel;
        hash_u16(&hash, frame->group_index);
        hash_u16(&hash, frame->picture.screen_offset);
        hash_u16(&hash, frame->picture.height);
        hash_u16(&hash, frame->picture.width);
        for (pixel = 0; pixel < frame->picture.pixel_count; ++pixel) {
            hash_byte(&hash, frame->picture.pixels[pixel]);
        }
        total_pixels += frame->picture.pixel_count;
    }
    printf("ANIM: colors=%u groups=%zu frames=%zu pixels=%zu hash=%016llx\n",
        (unsigned)archive.palette_count, archive.group_count, archive.frame_count,
        total_pixels, (unsigned long long)hash);
    success = archive.palette_count == 102 && archive.group_count == 100 &&
        archive.frame_count == 221 && total_pixels == 79620 &&
        hash == UINT64_C(0x461f021b2a6bfdea);
    sr_free_animation_archive(&archive);
    return success ? 0 : 1;
}
