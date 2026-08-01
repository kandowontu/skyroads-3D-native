#include "renderer_tables.h"

#include <string.h>

static uint16_t read_u16(const uint8_t *bytes) {
    return (uint16_t)(bytes[0] | ((uint16_t)bytes[1] << 8));
}

int sr_load_renderer_tables_from_exe(
    const uint8_t *bytes,
    size_t size,
    SrRendererTables *tables) {
    size_t header_size;
    size_t data_segment;
    size_t index;
    if (bytes == 0 || tables == 0 || size < 0x1cu ||
        bytes[0] != 'M' || bytes[1] != 'Z') return 0;
    header_size = (size_t)read_u16(bytes + 8u) * 16u;
    data_segment = header_size + 0x066eu * 16u;
    if (data_segment > size || 0x0b77u > size - data_segment) return 0;
    for (index = 0; index < SR_VISIBILITY_HEIGHT_COUNT; ++index) {
        tables->visibility_half_widths[index] =
            read_u16(bytes + data_segment + 0x044au + index * 2u);
    }
    memcpy(tables->shadows, bytes + data_segment + 0x065eu,
        sizeof(tables->shadows));
    return 1;
}

void sr_vga_renderer_state_init(SrVgaRendererState *state) {
    if (state != 0) memset(state, 0, sizeof(*state));
}
