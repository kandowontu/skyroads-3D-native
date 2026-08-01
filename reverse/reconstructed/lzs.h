#ifndef SKYROADS_RECOVERED_LZS_H
#define SKYROADS_RECOVERED_LZS_H

#include <stddef.h>
#include <stdint.h>

typedef enum SrLzsError {
    SR_LZS_OK = 0,
    SR_LZS_TRUNCATED,
    SR_LZS_BAD_WIDTH,
    SR_LZS_BAD_DISTANCE,
    SR_LZS_OUTPUT_OVERRUN
} SrLzsError;

typedef struct SrLzsResult {
    SrLzsError error;
    size_t input_consumed;
    size_t output_written;
} SrLzsResult;

/*
 * Semantic reconstruction of skyroads.exe 1000:6660.
 *
 * The input begins with three one-byte field widths in this order:
 * copy-count bits, short-distance bits, long-distance bits. The bitstream is
 * MSB-first. Unlike a guessed asset decoder, this interface deliberately
 * mirrors the executable routine, which consumes the widths itself.
 */
SrLzsResult sr_lzs_decompress(
    const uint8_t *input,
    size_t input_size,
    uint8_t *output,
    size_t output_size);

#endif
