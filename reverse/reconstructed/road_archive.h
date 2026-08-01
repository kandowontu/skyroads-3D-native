#ifndef SKYROADS_RECOVERED_ROAD_ARCHIVE_H
#define SKYROADS_RECOVERED_ROAD_ARCHIVE_H

#include <stddef.h>
#include <stdint.h>

typedef enum SrRoadArchiveError {
    SR_ROAD_ARCHIVE_OK = 0,
    SR_ROAD_ARCHIVE_TRUNCATED,
    SR_ROAD_ARCHIVE_BAD_INDEX,
    SR_ROAD_ARCHIVE_BAD_RECORD,
    SR_ROAD_ARCHIVE_OUT_OF_MEMORY,
    SR_ROAD_ARCHIVE_DECOMPRESSION_FAILED
} SrRoadArchiveError;

typedef struct SrRoadData {
    uint16_t gravity;
    uint16_t fuel;
    uint16_t oxygen;
    uint8_t palette[72 * 3];
    uint16_t *cells;
    size_t row_count;
} SrRoadData;

typedef struct SrRoadArchive {
    SrRoadData *roads;
    size_t road_count;
} SrRoadArchive;

/* Memory form of the exact roads.lzs loader at skyroads.exe 1000:55F8. */
SrRoadArchiveError sr_load_road_archive(
    const uint8_t *bytes,
    size_t size,
    SrRoadArchive *archive);

void sr_free_road_archive(SrRoadArchive *archive);

#endif
