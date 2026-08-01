#include "config.h"

#include <stdio.h>
#include <string.h>

int main(void) {
    SrConfigBlock config;
    memset(&config, 0, sizeof(config));

    if (!sr_update_config_checksum(&config) || config.checksum != 0x0210) {
        fputs("zeroed config checksum vector failed\n", stderr);
        return 1;
    }
    if (sr_update_config_checksum(&config)) {
        fputs("stable config checksum vector failed\n", stderr);
        return 1;
    }
    config.values[0] = 0x1234;
    if (!sr_update_config_checksum(&config) || config.checksum != 0x1444) {
        fputs("modified config checksum vector failed\n", stderr);
        return 1;
    }
    puts("Recovered SKYROADS.CFG checksum passed all vectors");
    return 0;
}
