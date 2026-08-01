#include "display_table.h"

#include <stdlib.h>
#include <string.h>

static uint16_t read_u16(const uint8_t *bytes) {
    return (uint16_t)(bytes[0] | ((uint16_t)bytes[1] << 8));
}

void sr_free_display_table(SrDisplayTable *table) {
    if (table == 0) return;
    free(table->offsets);
    free(table->data);
    memset(table, 0, sizeof(*table));
}

int sr_load_display_table(
    const uint8_t *bytes,
    size_t size,
    size_t entry_count,
    SrDisplayTable *table) {
    size_t index_bytes;
    size_t index;
    if (table == 0) return 0;
    memset(table, 0, sizeof(*table));
    if (bytes == 0 || entry_count > SIZE_MAX / 2u) return 0;
    index_bytes = entry_count * 2u;
    if (index_bytes > size) return 0;
    table->offsets = (uint16_t *)malloc(entry_count * sizeof(*table->offsets));
    table->data_size = size - index_bytes;
    table->data = (uint8_t *)malloc(table->data_size);
    if ((entry_count != 0 && table->offsets == 0) ||
        (table->data_size != 0 && table->data == 0)) {
        sr_free_display_table(table);
        return 0;
    }
    for (index = 0; index < entry_count; ++index) {
        table->offsets[index] = read_u16(bytes + index * 2u);
        if (table->offsets[index] >= table->data_size) {
            sr_free_display_table(table);
            return 0;
        }
    }
    memcpy(table->data, bytes + index_bytes, table->data_size);
    table->entry_count = entry_count;
    return 1;
}

int sr_get_display_sprite(
    const SrDisplayTable *table,
    size_t index,
    SrDisplaySprite *sprite) {
    size_t offset;
    size_t record_end;
    size_t pixel_count;
    if (table == 0 || sprite == 0 || index >= table->entry_count) return 0;
    offset = table->offsets[index];
    record_end = index + 1u < table->entry_count
        ? table->offsets[index + 1u] : table->data_size;
    if (record_end < offset || record_end - offset < 4u) return 0;
    pixel_count = (size_t)table->data[offset + 2u] * table->data[offset + 3u];
    if (pixel_count > record_end - offset - 4u) return 0;
    sprite->screen_offset = read_u16(table->data + offset);
    sprite->width = table->data[offset + 2u];
    sprite->height = table->data[offset + 3u];
    sprite->pixels = table->data + offset + 4u;
    sprite->pixel_count = pixel_count;
    return 1;
}
