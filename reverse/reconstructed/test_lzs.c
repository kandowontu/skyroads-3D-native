#include "lzs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t read_u16(const uint8_t *bytes) {
    return (uint16_t)(bytes[0] | ((uint16_t)bytes[1] << 8));
}

static uint64_t fnv1a64(const uint8_t *bytes, size_t size) {
    uint64_t hash = UINT64_C(1469598103934665603);
    size_t index;
    for (index = 0; index < size; ++index) {
        hash ^= bytes[index];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t fnv1a64_update(uint64_t hash, const uint8_t *bytes, size_t size) {
    size_t index;
    for (index = 0; index < size; ++index) {
        hash ^= bytes[index];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static int test_synthetic(void) {
    /* widths 5,8,10; literals A/B; short distance 2; copy count 4 */
    static const uint8_t packed[] = {5, 8, 10, 0xd0, 0x74, 0x20, 0x00, 0x80};
    uint8_t output[6] = {0};
    SrLzsResult decoded = sr_lzs_decompress(packed, sizeof(packed), output, sizeof(output));
    return decoded.error == SR_LZS_OK && memcmp(output, "ABABAB", 6) == 0;
}

static int test_intro(const char *path) {
    FILE *stream = fopen(path, "rb");
    uint8_t *source;
    uint8_t *output;
    long file_size;
    size_t pict;
    size_t expected;
    SrLzsResult decoded;
    int ok;

    if (stream == NULL) return 0;
    fseek(stream, 0, SEEK_END);
    file_size = ftell(stream);
    fseek(stream, 0, SEEK_SET);
    source = (uint8_t *)malloc((size_t)file_size);
    if (source == NULL || fread(source, 1, (size_t)file_size, stream) != (size_t)file_size) {
        fclose(stream);
        free(source);
        return 0;
    }
    fclose(stream);

    for (pict = 0; pict + 13 <= (size_t)file_size; ++pict) {
        if (memcmp(source + pict, "PICT", 4) == 0) break;
    }
    if (pict + 13 > (size_t)file_size) {
        free(source);
        return 0;
    }

    expected = (size_t)read_u16(source + pict + 6) * read_u16(source + pict + 8);
    output = (uint8_t *)malloc(expected);
    if (output == NULL) {
        free(source);
        return 0;
    }
    decoded = sr_lzs_decompress(
        source + pict + 10,
        (size_t)file_size - pict - 10,
        output,
        expected);
    ok = decoded.error == SR_LZS_OK && expected == 64000 &&
        fnv1a64(output, expected) == UINT64_C(0x06f0c4581ec036b0);
    free(output);
    free(source);
    return ok;
}

static int test_all_roads(const char *path) {
    FILE *stream = fopen(path, "rb");
    uint8_t *source;
    long file_size;
    size_t first_offset;
    size_t road_count;
    size_t road;
    size_t total_output = 0;
    uint64_t hash = UINT64_C(1469598103934665603);

    if (stream == NULL) return 0;
    fseek(stream, 0, SEEK_END);
    file_size = ftell(stream);
    fseek(stream, 0, SEEK_SET);
    source = (uint8_t *)malloc((size_t)file_size);
    if (source == NULL || fread(source, 1, (size_t)file_size, stream) != (size_t)file_size) {
        fclose(stream);
        free(source);
        return 0;
    }
    fclose(stream);

    first_offset = read_u16(source);
    road_count = first_offset / 4;
    if (first_offset == 0 || first_offset % 4 != 0 || road_count != 31) {
        free(source);
        return 0;
    }

    for (road = 0; road < road_count; ++road) {
        size_t entry = road * 4;
        size_t offset = read_u16(source + entry);
        size_t output_size = read_u16(source + entry + 2);
        size_t next_offset = road + 1 < road_count
            ? read_u16(source + entry + 4)
            : (size_t)file_size;
        uint8_t *output;
        SrLzsResult decoded;

        if (offset + 225 > next_offset || next_offset > (size_t)file_size) {
            free(source);
            return 0;
        }
        output = (uint8_t *)malloc(output_size);
        if (output == NULL) {
            free(source);
            return 0;
        }
        decoded = sr_lzs_decompress(
            source + offset + 222,
            next_offset - offset - 222,
            output,
            output_size);
        if (decoded.error != SR_LZS_OK) {
            free(output);
            free(source);
            return 0;
        }
        hash = fnv1a64_update(hash, output, output_size);
        total_output += output_size;
        free(output);
    }

    free(source);
    return total_output == 60872 && hash == UINT64_C(0x9adc0bfc11e5c957);
}

static int test_all_trekdat_records(const char *path) {
    FILE *stream = fopen(path, "rb");
    uint8_t *source;
    long file_size;
    size_t cursor = 0;
    size_t record_count = 0;
    size_t total_output = 0;
    uint64_t hash = UINT64_C(1469598103934665603);

    if (stream == NULL) return 0;
    fseek(stream, 0, SEEK_END);
    file_size = ftell(stream);
    fseek(stream, 0, SEEK_SET);
    source = (uint8_t *)malloc((size_t)file_size);
    if (source == NULL || fread(source, 1, (size_t)file_size, stream) != (size_t)file_size) {
        fclose(stream);
        free(source);
        return 0;
    }
    fclose(stream);

    while (cursor < (size_t)file_size && record_count < 8) {
        size_t allocation_size;
        size_t output_size;
        uint8_t *output;
        SrLzsResult decoded;

        if (cursor + 7 > (size_t)file_size) {
            free(source);
            return 0;
        }
        allocation_size = read_u16(source + cursor);
        output_size = read_u16(source + cursor + 2);
        if (allocation_size < output_size || allocation_size - output_size < 2) {
            free(source);
            return 0;
        }
        output = (uint8_t *)malloc(output_size);
        if (output == NULL) {
            free(source);
            return 0;
        }
        decoded = sr_lzs_decompress(
            source + cursor + 4,
            (size_t)file_size - cursor - 4,
            output,
            output_size);
        if (decoded.error != SR_LZS_OK) {
            free(output);
            free(source);
            return 0;
        }
        hash = fnv1a64_update(hash, output, output_size);
        total_output += output_size;
        cursor += 4 + decoded.input_consumed;
        ++record_count;
        free(output);
    }

    free(source);
    return record_count == 8 && cursor == (size_t)file_size && total_output == 152842 &&
        hash == UINT64_C(0x55116550760be181);
}

int main(int argc, char **argv) {
    if (!test_synthetic()) {
        fputs("synthetic LZS test failed\n", stderr);
        return 1;
    }
    if (argc != 4 || !test_intro(argv[1])) {
        fputs("INTRO.LZS executable-equivalence vector failed\n", stderr);
        return 1;
    }
    if (!test_all_roads(argv[2])) {
        fputs("all-roads LZS executable-equivalence vector failed\n", stderr);
        return 1;
    }
    if (!test_all_trekdat_records(argv[3])) {
        fputs("all-record TREKDAT.LZS executable-equivalence vector failed\n", stderr);
        return 1;
    }
    puts("Recovered LZS passed synthetic, INTRO, all 31 roads, and all 8 TREKDAT records");
    return 0;
}
