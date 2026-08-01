#include "road_archive.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint64_t hash_u16(uint64_t hash, uint16_t value) {
    hash ^= value & 0xffu;
    hash *= UINT64_C(1099511628211);
    hash ^= value >> 8;
    hash *= UINT64_C(1099511628211);
    return hash;
}

int main(int argc, char **argv) {
    FILE *stream;
    long file_size;
    uint8_t *bytes;
    SrRoadArchive archive;
    uint64_t hash = UINT64_C(1469598103934665603);
    size_t index;

    if (argc != 2 || (stream = fopen(argv[1], "rb")) == 0) {
        fputs("roads.lzs path required\n", stderr);
        return 1;
    }
    fseek(stream, 0, SEEK_END);
    file_size = ftell(stream);
    fseek(stream, 0, SEEK_SET);
    bytes = (uint8_t *)malloc((size_t)file_size);
    if (bytes == 0 || fread(bytes, 1, (size_t)file_size, stream) != (size_t)file_size) {
        fclose(stream);
        free(bytes);
        return 1;
    }
    fclose(stream);
    if (sr_load_road_archive(bytes, (size_t)file_size, &archive) != SR_ROAD_ARCHIVE_OK) {
        free(bytes);
        return 1;
    }
    free(bytes);

    if (archive.road_count != 31 || archive.roads[0].row_count != 160 ||
        archive.roads[1].row_count != 55 || archive.roads[0].gravity != 8 ||
        archive.roads[0].fuel != 130 || archive.roads[0].oxygen != 60) {
        sr_free_road_archive(&archive);
        return 1;
    }
    for (index = 8; index <= 10; ++index) {
        size_t column;
        printf("road 0 row %zu:", index);
        for (column = 0; column < 7; ++column) {
            printf(" %04x", archive.roads[0].cells[index * 7u + column]);
        }
        putchar('\n');
    }
    for (index = 0; index < archive.road_count; ++index) {
        size_t cell;
        for (cell = 0; cell < archive.roads[index].row_count * 7u; ++cell) {
            hash = hash_u16(hash, archive.roads[index].cells[cell]);
        }
    }
    if (hash != UINT64_C(0x9adc0bfc11e5c957)) {
        fprintf(stderr, "road-cell hash mismatch: %016llx\n", (unsigned long long)hash);
        sr_free_road_archive(&archive);
        return 1;
    }
    sr_free_road_archive(&archive);
    puts("road archive loader passed all 31 original records");
    return 0;
}
