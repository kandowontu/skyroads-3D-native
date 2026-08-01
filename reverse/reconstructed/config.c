#include "config.h"

int sr_update_config_checksum(SrConfigBlock *config) {
    uint16_t checksum = 0;
    uint16_t index;

    for (index = 1; index < 33; ++index) {
        checksum = (uint16_t)(checksum + (uint16_t)(config->values[index - 1] ^ index));
    }
    if (checksum == config->checksum) {
        return 0;
    }
    config->checksum = checksum;
    return 1;
}
