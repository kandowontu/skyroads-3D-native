#ifndef SKYROADS_RECOVERED_CAR_SPRITES_H
#define SKYROADS_RECOVERED_CAR_SPRITES_H

#include <stddef.h>
#include <stdint.h>

typedef struct SrCarSprites {
    uint8_t palette[20 * 3];
    uint8_t *pixels;
    size_t pixel_count;
    size_t frame_count;
} SrCarSprites;

/* Exact cars.lzs CMAP/PICT payload used by skyroads.exe 1000:554B. */
int sr_load_car_sprites(const uint8_t *bytes, size_t size, SrCarSprites *sprites);
void sr_free_car_sprites(SrCarSprites *sprites);

#endif
