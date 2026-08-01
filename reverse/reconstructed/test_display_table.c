#include "display_table.h"

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

int main(int argc, char **argv) {
    static const size_t counts[3] = {10, 10, 34};
    static const size_t expected_sizes[3] = {375, 387, 3835};
    uint64_t hash = UINT64_C(1469598103934665603);
    size_t table_index;
    if (argc != 4) return 1;
    for (table_index = 0; table_index < 3; ++table_index) {
        uint8_t *bytes;
        size_t size;
        size_t index;
        SrDisplayTable table;
        bytes = read_file(argv[table_index + 1], &size);
        if (bytes == 0 || !sr_load_display_table(
                bytes, size, counts[table_index], &table)) {
            free(bytes); return 1;
        }
        free(bytes);
        if (table.data_size != expected_sizes[table_index]) return 1;
        for (index = 0; index < table.entry_count; ++index) {
            SrDisplaySprite sprite;
            size_t record_end = index + 1u < table.entry_count
                ? table.offsets[index + 1u] : table.data_size;
            if (!sr_get_display_sprite(&table, index, &sprite) ||
                table.offsets[index] + 4u + sprite.pixel_count != record_end) return 1;
            hash_byte(&hash, (uint8_t)table.offsets[index]);
            hash_byte(&hash, (uint8_t)(table.offsets[index] >> 8));
        }
        for (index = 0; index < table.data_size; ++index) {
            hash_byte(&hash, table.data[index]);
        }
        printf("%s: entries=%zu data=%zu final_offset=%u\n", argv[table_index + 1],
            table.entry_count, table.data_size,
            (unsigned)table.offsets[table.entry_count - 1u]);
        sr_free_display_table(&table);
    }
    printf("display tables hash=%016llx\n", (unsigned long long)hash);
    return hash == UINT64_C(0xe625dc8e752a35a4) ? 0 : 1;
}
