#ifndef SKYROADS_RECOVERED_TREK_ARCHIVE_H
#define SKYROADS_RECOVERED_TREK_ARCHIVE_H

#include <stddef.h>
#include <stdint.h>

typedef enum SrTrekArchiveError {
    SR_TREK_ARCHIVE_OK = 0,
    SR_TREK_ARCHIVE_TRUNCATED,
    SR_TREK_ARCHIVE_BAD_RECORD,
    SR_TREK_ARCHIVE_OUT_OF_MEMORY,
    SR_TREK_ARCHIVE_DECOMPRESSION_FAILED
} SrTrekArchiveError;

typedef struct SrTrekRecord {
    uint8_t *bytes;
    size_t size;
} SrTrekRecord;

typedef struct SrTrekArchive {
    SrTrekRecord records[8];
    size_t record_count;
} SrTrekArchive;

/* Exact load/expand pipeline at skyroads.exe 1000:00BB and 1000:3A7A. */
SrTrekArchiveError sr_load_trek_archive(
    const uint8_t *bytes,
    size_t size,
    SrTrekArchive *archive);

void sr_free_trek_archive(SrTrekArchive *archive);

#endif
