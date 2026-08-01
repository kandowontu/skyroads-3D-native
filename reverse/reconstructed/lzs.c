#include "lzs.h"

typedef struct BitReader {
    const uint8_t *bytes;
    size_t size;
    size_t byte_index;
    unsigned bit_index;
} BitReader;

static int read_bits(BitReader *reader, unsigned count, uint32_t *value) {
    unsigned index;
    uint32_t result = 0;

    for (index = 0; index < count; ++index) {
        if (reader->byte_index >= reader->size) {
            return 0;
        }
        result = (result << 1) |
            ((reader->bytes[reader->byte_index] >> (7 - reader->bit_index)) & 1u);
        if (++reader->bit_index == 8) {
            reader->bit_index = 0;
            ++reader->byte_index;
        }
    }
    *value = result;
    return 1;
}

static size_t consumed_bytes(const BitReader *reader) {
    return reader->byte_index + (reader->bit_index != 0);
}

static SrLzsResult result(
    SrLzsError error,
    const BitReader *reader,
    size_t output_written) {
    SrLzsResult decoded;
    decoded.error = error;
    decoded.input_consumed = consumed_bytes(reader);
    decoded.output_written = output_written;
    return decoded;
}

SrLzsResult sr_lzs_decompress(
    const uint8_t *input,
    size_t input_size,
    uint8_t *output,
    size_t output_size) {
    BitReader reader;
    uint32_t count_bits;
    uint32_t short_distance_bits;
    uint32_t long_distance_bits;
    size_t written = 0;

    reader.bytes = input;
    reader.size = input_size;
    reader.byte_index = 0;
    reader.bit_index = 0;

    /* 1000:666C..6688: the original reads these as whole bytes. */
    if (!read_bits(&reader, 8, &count_bits) ||
        !read_bits(&reader, 8, &short_distance_bits) ||
        !read_bits(&reader, 8, &long_distance_bits)) {
        return result(SR_LZS_TRUNCATED, &reader, written);
    }
    if (count_bits > 16 || short_distance_bits > 16 || long_distance_bits > 16) {
        return result(SR_LZS_BAD_WIDTH, &reader, written);
    }

    while (written < output_size) {
        uint32_t first;
        uint32_t second;

        if (!read_bits(&reader, 1, &first)) {
            return result(SR_LZS_TRUNCATED, &reader, written);
        }
        if (first == 0) {
            uint32_t encoded_distance;
            uint32_t encoded_count;
            size_t distance;
            size_t count;
            size_t index;

            if (!read_bits(&reader, (unsigned)short_distance_bits, &encoded_distance) ||
                !read_bits(&reader, (unsigned)count_bits, &encoded_count)) {
                return result(SR_LZS_TRUNCATED, &reader, written);
            }
            distance = (size_t)encoded_distance + 2;
            count = (size_t)encoded_count + 2;
            if (distance > written) {
                return result(SR_LZS_BAD_DISTANCE, &reader, written);
            }
            if (count > output_size - written) {
                return result(SR_LZS_OUTPUT_OVERRUN, &reader, written);
            }
            for (index = 0; index < count; ++index) {
                output[written] = output[written - distance];
                ++written;
            }
            continue;
        }

        if (!read_bits(&reader, 1, &second)) {
            return result(SR_LZS_TRUNCATED, &reader, written);
        }
        if (second == 0) {
            uint32_t encoded_distance;
            uint32_t encoded_count;
            size_t distance;
            size_t count;
            size_t index;

            if (!read_bits(&reader, (unsigned)long_distance_bits, &encoded_distance) ||
                !read_bits(&reader, (unsigned)count_bits, &encoded_count)) {
                return result(SR_LZS_TRUNCATED, &reader, written);
            }
            distance = (size_t)encoded_distance + 2 +
                ((size_t)1 << short_distance_bits);
            count = (size_t)encoded_count + 2;
            if (distance > written) {
                return result(SR_LZS_BAD_DISTANCE, &reader, written);
            }
            if (count > output_size - written) {
                return result(SR_LZS_OUTPUT_OVERRUN, &reader, written);
            }
            for (index = 0; index < count; ++index) {
                output[written] = output[written - distance];
                ++written;
            }
            continue;
        }

        {
            uint32_t literal;
            if (!read_bits(&reader, 8, &literal)) {
                return result(SR_LZS_TRUNCATED, &reader, written);
            }
            output[written++] = (uint8_t)literal;
        }
    }

    /* 1000:65F3 discards the unused low bits of the current source byte. */
    if (reader.bit_index != 0) {
        reader.bit_index = 0;
        ++reader.byte_index;
    }
    return result(SR_LZS_OK, &reader, written);
}
