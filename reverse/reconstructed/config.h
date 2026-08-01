#ifndef SKYROADS_RECOVERED_CONFIG_H
#define SKYROADS_RECOVERED_CONFIG_H

#include <stdint.h>

typedef struct SrConfigBlock {
    uint16_t checksum;       /* DS:4524 */
    uint16_t values[32];     /* DS:4526..4565 */
} SrConfigBlock;

#ifdef __cplusplus
static_assert(sizeof(SrConfigBlock) == 0x42, "SKYROADS.CFG block must be 0x42 bytes");
#else
_Static_assert(sizeof(SrConfigBlock) == 0x42, "SKYROADS.CFG block must be 0x42 bytes");
#endif

/* Exact reconstruction of skyroads.exe 1000:56C3. Returns 1 when changed. */
int sr_update_config_checksum(SrConfigBlock *config);

#endif
