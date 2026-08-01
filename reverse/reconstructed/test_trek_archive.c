#include "trek_archive.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint64_t hash_bytes(uint64_t hash, const uint8_t *bytes, size_t size) {
    size_t index;
    for (index = 0; index < size; ++index) {
        hash ^= bytes[index];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

int main(int argc, char **argv) {
    FILE *stream;
    long file_size;
    uint8_t *bytes;
    SrTrekArchive archive;
    uint64_t hash = UINT64_C(1469598103934665603);
    size_t total = 0;
    size_t index;

    if ((argc != 2 && argc != 3) || (stream = fopen(argv[1], "rb")) == 0) return 1;
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
    if (sr_load_trek_archive(bytes, (size_t)file_size, &archive) != SR_TREK_ARCHIVE_OK) {
        free(bytes);
        return 1;
    }
    free(bytes);
    for (index = 0; index < archive.record_count; ++index) {
        printf("record %zu: %zu bytes\n", index, archive.records[index].size);
        hash = hash_bytes(hash, archive.records[index].bytes, archive.records[index].size);
        total += archive.records[index].size;
    }
    printf("expanded TREKDAT: records=%zu bytes=%zu hash=%016llx\n",
        archive.record_count, total, (unsigned long long)hash);
    if (archive.record_count != 8 || total != 210127 ||
        archive.records[0].size != 24716 ||
        hash != UINT64_C(0xa1ffaf717cadea91)) {
        sr_free_trek_archive(&archive);
        return 1;
    }
    if (argc == 3) {
        stream = fopen(argv[2], "wb");
        if (stream == 0 || fwrite(archive.records[1].bytes, 1,
                archive.records[1].size, stream) != archive.records[1].size) {
            if (stream != 0) fclose(stream);
            sr_free_trek_archive(&archive);
            return 1;
        }
        fclose(stream);
    }
    sr_free_trek_archive(&archive);
    return 0;
}
