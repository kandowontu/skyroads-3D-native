#include "road_archive.h"

#include <stdlib.h>
#include <string.h>

#include "lzs.h"

static uint16_t read_u16(const uint8_t *bytes) {
    return (uint16_t)(bytes[0] | ((uint16_t)bytes[1] << 8));
}

void sr_free_road_archive(SrRoadArchive *archive) {
    size_t index;
    if (archive == 0) {
        return;
    }
    for (index = 0; index < archive->road_count; ++index) {
        free(archive->roads[index].cells);
    }
    free(archive->roads);
    archive->roads = 0;
    archive->road_count = 0;
}

SrRoadArchiveError sr_load_road_archive(
    const uint8_t *bytes,
    size_t size,
    SrRoadArchive *archive) {
    size_t first_offset;
    size_t road_count;
    size_t index;

    if (archive == 0) {
        return SR_ROAD_ARCHIVE_BAD_RECORD;
    }
    archive->roads = 0;
    archive->road_count = 0;
    if (bytes == 0 || size < 4) {
        return SR_ROAD_ARCHIVE_TRUNCATED;
    }
    first_offset = read_u16(bytes);
    if (first_offset == 0 || (first_offset & 3u) != 0 || first_offset > size) {
        return SR_ROAD_ARCHIVE_BAD_INDEX;
    }
    road_count = first_offset / 4u;
    if (road_count > size / 4u) {
        return SR_ROAD_ARCHIVE_BAD_INDEX;
    }
    archive->roads = (SrRoadData *)calloc(road_count, sizeof(*archive->roads));
    if (archive->roads == 0) {
        return SR_ROAD_ARCHIVE_OUT_OF_MEMORY;
    }
    archive->road_count = road_count;

    for (index = 0; index < road_count; ++index) {
        size_t entry = index * 4u;
        size_t offset = read_u16(bytes + entry);
        size_t output_size = read_u16(bytes + entry + 2u);
        size_t next_offset = index + 1u < road_count
            ? read_u16(bytes + entry + 4u)
            : size;
        SrRoadData *road = &archive->roads[index];
        uint8_t *decoded;
        SrLzsResult result;
        size_t cell;

        if (offset < first_offset || next_offset > size || offset > next_offset ||
            next_offset - offset < 225u || output_size == 0 ||
            output_size % 14u != 0) {
            sr_free_road_archive(archive);
            return SR_ROAD_ARCHIVE_BAD_RECORD;
        }
        road->gravity = read_u16(bytes + offset);
        road->fuel = read_u16(bytes + offset + 2u);
        road->oxygen = read_u16(bytes + offset + 4u);
        memcpy(road->palette, bytes + offset + 6u, sizeof(road->palette));
        decoded = (uint8_t *)malloc(output_size);
        road->cells = (uint16_t *)malloc(
            (output_size / 2u) * sizeof(*road->cells));
        if (decoded == 0 || road->cells == 0) {
            free(decoded);
            sr_free_road_archive(archive);
            return SR_ROAD_ARCHIVE_OUT_OF_MEMORY;
        }
        result = sr_lzs_decompress(
            bytes + offset + 222u,
            next_offset - offset - 222u,
            decoded,
            output_size);
        if (result.error != SR_LZS_OK || result.output_written != output_size) {
            free(decoded);
            sr_free_road_archive(archive);
            return SR_ROAD_ARCHIVE_DECOMPRESSION_FAILED;
        }
        for (cell = 0; cell < output_size / 2u; ++cell) {
            road->cells[cell] = read_u16(decoded + cell * 2u);
        }
        road->row_count = output_size / 14u;
        free(decoded);
    }
    return SR_ROAD_ARCHIVE_OK;
}
