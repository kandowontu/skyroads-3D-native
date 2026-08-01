#ifndef SKYROADS_RECOVERED_DISPLAY_TABLE_H
#define SKYROADS_RECOVERED_DISPLAY_TABLE_H

#include <stddef.h>
#include <stdint.h>

typedef struct SrDisplayTable {
    uint16_t *offsets;
    size_t entry_count;
    uint8_t *data;
    size_t data_size;
} SrDisplayTable;

typedef struct SrDisplaySprite {
    uint16_t screen_offset;
    uint8_t width;
    uint8_t height;
    const uint8_t *pixels;
    size_t pixel_count;
} SrDisplaySprite;

/* Memory form of the file split performed by load_display_table at 5465. */
int sr_load_display_table(
    const uint8_t *bytes,
    size_t size,
    size_t entry_count,
    SrDisplayTable *table);

void sr_free_display_table(SrDisplayTable *table);

/* Record layout consumed by dashboard sprite renderer 0EDF. */
int sr_get_display_sprite(
    const SrDisplayTable *table,
    size_t index,
    SrDisplaySprite *sprite);

#endif
