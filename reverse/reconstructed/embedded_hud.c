#include "embedded_hud.h"

#include <string.h>

static uint16_t read_u16(const uint8_t *bytes) {
    return (uint16_t)(bytes[0] | ((uint16_t)bytes[1] << 8));
}

int sr_load_embedded_hud_from_exe(
    const uint8_t *bytes,
    size_t size,
    SrEmbeddedHud *hud) {
    size_t header_size;
    size_t data_segment;
    size_t index;
    if (bytes == 0 || hud == 0 || size < 0x1cu ||
        bytes[0] != 'M' || bytes[1] != 'Z') return 0;
    header_size = (size_t)read_u16(bytes + 8u) * 16u;
    data_segment = header_size + 0x066eu * 16u;
    if (data_segment > size || 0x0310u > size - data_segment) return 0;
    memcpy(hud->digits, bytes + data_segment + 0x013cu, sizeof(hud->digits));
    memcpy(hud->jumpmaster, bytes + data_segment + 0x0204u,
        sizeof(hud->jumpmaster));
    for (index = 0; index < 4; ++index) {
        hud->decimal_divisors[index] =
            read_u16(bytes + data_segment + 0x0308u + index * 2u);
    }
    return hud->decimal_divisors[0] == 1 &&
        hud->decimal_divisors[1] == 10 &&
        hud->decimal_divisors[2] == 100 &&
        hud->decimal_divisors[3] == 1000;
}
