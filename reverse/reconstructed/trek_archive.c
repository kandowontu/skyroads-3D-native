#include "trek_archive.h"

#include <stdlib.h>
#include <string.h>

#include "lzs.h"

enum {
    SR_TREK_POINTER_TABLE_BYTES = 0x270,
    SR_TREK_SHAPE_COUNT = 0x410
};

static uint16_t read_u16(const uint8_t *bytes) {
    return (uint16_t)(bytes[0] | ((uint16_t)bytes[1] << 8));
}

static int expand_record(uint8_t *buffer, size_t size, size_t source_offset) {
    size_t source;
    size_t destination;
    unsigned shape;

    if (source_offset > size || size - source_offset < SR_TREK_POINTER_TABLE_BYTES) {
        return 0;
    }
    memmove(buffer, buffer + source_offset, SR_TREK_POINTER_TABLE_BYTES);
    source = source_offset + SR_TREK_POINTER_TABLE_BYTES;
    destination = SR_TREK_POINTER_TABLE_BYTES;
    for (shape = 0; shape < SR_TREK_SHAPE_COUNT; ++shape) {
        if (source > size || size - source < 4 ||
            destination > size || size - destination < 4) {
            return 0;
        }
        buffer[destination++] = buffer[source++];
        buffer[destination++] = buffer[source++];
        buffer[destination++] = buffer[source++];
        for (;;) {
            uint8_t marker;
            if (source >= size || destination >= size) {
                return 0;
            }
            marker = buffer[source++];
            buffer[destination++] = marker;
            if (marker == 0xffu) {
                break;
            }
            if (source >= size || size - destination < 2) {
                return 0;
            }
            buffer[destination++] = buffer[source++];
            buffer[destination++] = 0;
        }
    }
    return destination == size;
}

void sr_free_trek_archive(SrTrekArchive *archive) {
    size_t index;
    if (archive == 0) return;
    for (index = 0; index < archive->record_count; ++index) {
        free(archive->records[index].bytes);
        archive->records[index].bytes = 0;
        archive->records[index].size = 0;
    }
    archive->record_count = 0;
}

SrTrekArchiveError sr_load_trek_archive(
    const uint8_t *bytes,
    size_t size,
    SrTrekArchive *archive) {
    size_t cursor = 0;

    if (archive == 0) return SR_TREK_ARCHIVE_BAD_RECORD;
    memset(archive, 0, sizeof(*archive));
    if (bytes == 0) return SR_TREK_ARCHIVE_TRUNCATED;
    while (cursor < size) {
        uint16_t allocation_size;
        uint16_t output_size;
        size_t load_offset;
        uint8_t *record;
        SrLzsResult decoded;

        if (archive->record_count >= 8 || size - cursor < 7) {
            sr_free_trek_archive(archive);
            return SR_TREK_ARCHIVE_BAD_RECORD;
        }
        allocation_size = read_u16(bytes + cursor);
        output_size = read_u16(bytes + cursor + 2u);
        if (allocation_size < output_size || output_size < SR_TREK_POINTER_TABLE_BYTES) {
            sr_free_trek_archive(archive);
            return SR_TREK_ARCHIVE_BAD_RECORD;
        }
        load_offset = (size_t)allocation_size - output_size;
        record = (uint8_t *)calloc(allocation_size, 1);
        if (record == 0) {
            sr_free_trek_archive(archive);
            return SR_TREK_ARCHIVE_OUT_OF_MEMORY;
        }
        record[0] = (uint8_t)load_offset;
        record[1] = (uint8_t)(load_offset >> 8);
        decoded = sr_lzs_decompress(
            bytes + cursor + 4u,
            size - cursor - 4u,
            record + load_offset,
            output_size);
        if (decoded.error != SR_LZS_OK || decoded.output_written != output_size) {
            free(record);
            sr_free_trek_archive(archive);
            return SR_TREK_ARCHIVE_DECOMPRESSION_FAILED;
        }
        if (!expand_record(record, allocation_size, load_offset)) {
            free(record);
            sr_free_trek_archive(archive);
            return SR_TREK_ARCHIVE_BAD_RECORD;
        }
        archive->records[archive->record_count].bytes = record;
        archive->records[archive->record_count].size = allocation_size;
        ++archive->record_count;
        cursor += 4u + decoded.input_consumed;
    }
    if (archive->record_count != 8) {
        sr_free_trek_archive(archive);
        return SR_TREK_ARCHIVE_BAD_RECORD;
    }
    return SR_TREK_ARCHIVE_OK;
}
