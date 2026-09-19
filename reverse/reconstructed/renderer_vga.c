#include "renderer_vga.h"

#include <string.h>

enum {
    VGA_ROAD_VIEWPORT_OFFSET = 0x2800,
    VGA_ROAD_VIEWPORT_END = 0xac80
};

static const uint8_t road_delta_classes[8] = {1, 2, 3, 3, 4, 4, 1, 1};

/* DS:0322, four hardware mappings for all 74 road-descriptor color indices. */
static const uint8_t color_mapping[74 * 4] = {
    0x00,0x00,0x00,0x00, 0x01,0x01,0x06,0x06, 0x02,0x02,0x02,0x02, 0x03,0x03,0x06,0x06,
    0x04,0x04,0x06,0x06, 0x05,0x05,0x06,0x06, 0x06,0x06,0x06,0x06, 0x07,0x07,0x06,0x06,
    0x08,0x08,0x08,0x08, 0x09,0x09,0x09,0x09, 0x0a,0x0a,0x0a,0x0a, 0x0b,0x0b,0x06,0x06,
    0x0c,0x0c,0x0c,0x0c, 0x0d,0x0d,0x06,0x06, 0x0e,0x0e,0x06,0x06, 0x0f,0x0f,0x06,0x06,
    0x10,0x10,0x0b,0x0b, 0x11,0x11,0x0b,0x0b, 0x12,0x12,0x0b,0x0b, 0x13,0x13,0x0b,0x0b,
    0x14,0x14,0x0b,0x0b, 0x15,0x15,0x0b,0x0b, 0x16,0x16,0x0b,0x0b, 0x17,0x17,0x0b,0x0b,
    0x18,0x18,0x0b,0x0b, 0x19,0x19,0x0b,0x0b, 0x1a,0x1a,0x0b,0x0b, 0x1b,0x1b,0x0b,0x0b,
    0x1c,0x1c,0x0b,0x0b, 0x1d,0x1d,0x0b,0x0b, 0x1e,0x1e,0x0b,0x0b, 0x1f,0x2e,0x03,0x08,
    0x20,0x2f,0x03,0x08, 0x21,0x30,0x03,0x08, 0x22,0x31,0x03,0x08, 0x23,0x32,0x03,0x08,
    0x24,0x33,0x03,0x08, 0x25,0x34,0x03,0x08, 0x26,0x35,0x03,0x08, 0x27,0x36,0x03,0x08,
    0x28,0x37,0x03,0x08, 0x29,0x38,0x03,0x08, 0x2a,0x39,0x03,0x08, 0x2b,0x3a,0x03,0x08,
    0x2c,0x3b,0x03,0x08, 0x2d,0x3c,0x03,0x08, 0x2e,0x00,0x00,0x00, 0x2f,0x00,0x00,0x00,
    0x30,0x00,0x00,0x00, 0x31,0x00,0x00,0x00, 0x32,0x00,0x00,0x00, 0x33,0x00,0x00,0x00,
    0x34,0x00,0x00,0x00, 0x35,0x00,0x00,0x00, 0x36,0x00,0x00,0x00, 0x37,0x00,0x00,0x00,
    0x38,0x00,0x00,0x00, 0x39,0x00,0x00,0x00, 0x3a,0x00,0x00,0x00, 0x3b,0x00,0x00,0x00,
    0x3c,0x00,0x00,0x00, 0x3d,0x3d,0x07,0x07, 0x3e,0x3e,0x0b,0x0b, 0x3f,0x40,0x03,0x08,
    0x40,0x00,0x00,0x00, 0x41,0x41,0x08,0x08, 0x42,0x42,0x0b,0x0b, 0x43,0x43,0x08,0x08,
    0x47,0x46,0x02,0x0a, 0x46,0x45,0x02,0x0a, 0x45,0x44,0x02,0x0a, 0x44,0x45,0x0a,0x02,
    0x45,0x46,0x0a,0x02, 0x46,0x47,0x0a,0x02
};

typedef struct DrawContext {
    SrTrekRecord *record;
    const uint16_t *cells;
    size_t row_count;
    uint8_t *framebuffer;
    size_t pointer_base;
    int row;
    int column;
    unsigned direction;
    unsigned pointer_relative;
    unsigned shape_ordinal;
    SrVgaRendererState *state;
} DrawContext;

static uint16_t read_u16(const uint8_t *bytes) {
    return (uint16_t)(bytes[0] | ((uint16_t)bytes[1] << 8));
}

static uint16_t road_cell(const DrawContext *context, int row, int column) {
    if (row < 0 || column < 0 || column >= 7 || (size_t)row >= context->row_count) {
        return 0;
    }
    return context->cells[(size_t)row * 7u + (size_t)column];
}

static unsigned road_type(uint16_t cell) {
    return (unsigned)(cell >> 8);
}

static unsigned road_delta_class(uint16_t cell) {
    return road_delta_classes[(cell >> 8) & 7u];
}

static int next_shape(
    const SrTrekRecord *record,
    size_t *shape) {
    size_t cursor = *shape;
    if (cursor + 3u > record->size) return 0;
    cursor += 3u;
    while (cursor < record->size && record->bytes[cursor] != 0xffu) {
        if (record->size - cursor < 3u) return 0;
        cursor += 3u;
    }
    if (cursor >= record->size) return 0;
    *shape = cursor + 1u;
    return 1;
}

static int shape_at(
    const SrTrekRecord *record,
    size_t pointer_offset,
    unsigned shape_index,
    size_t *shape) {
    unsigned index;
    if (pointer_offset + 2u > 0x270u ||
        pointer_offset + 2u > record->size) return 0;
    *shape = read_u16(record->bytes + pointer_offset);
    if (*shape >= record->size) return 0;
    for (index = 0; index < shape_index; ++index) {
        if (!next_shape(record, shape)) return 0;
    }
    return *shape + 3u <= record->size;
}

static void copy_word_aligned_span(
    uint8_t *destination,
    const uint8_t *source,
    uint32_t offset,
    uint32_t byte_count) {
    if (offset >= SR_VGA_FRAMEBUFFER_SIZE) return;
    if (byte_count > SR_VGA_FRAMEBUFFER_SIZE - offset) {
        byte_count = SR_VGA_FRAMEBUFFER_SIZE - offset;
    }
    memcpy(destination + offset, source + offset, byte_count);
}

/* 1000:38A3: copy the background-covered words for one old/new TREK shape. */
static int transfer_background_shape(
    const SrTrekRecord *old_record,
    const SrTrekRecord *new_record,
    uint16_t code,
    int depth,
    unsigned slot,
    unsigned direction,
    size_t phase_pointer_offset,
    uint8_t *road_buffer,
    const uint8_t *background) {
    size_t pointer_offset;
    size_t old_shape;
    size_t new_shape;
    size_t run;
    unsigned shape_index = code & 0xffu;
    uint16_t old_base;
    uint16_t new_base;

    pointer_offset = (size_t)((code & 0x7fffu) >> 7) +
        (size_t)(((11 - depth) * 4 + 4 - (int)slot) * 0x0c) +
        phase_pointer_offset;
    if (!shape_at(old_record, pointer_offset, shape_index, &old_shape) ||
        !shape_at(new_record, pointer_offset - phase_pointer_offset,
            shape_index, &new_shape)) return 0;
    old_base = read_u16(old_record->bytes + old_shape + 1u);
    new_base = read_u16(new_record->bytes + new_shape + 1u);
    run = new_shape + 3u;

    if ((code & 0x8000u) == 0) {
        if (new_record->bytes[run] == 0xffu) return 1;
        while (new_base < old_base) {
            if (new_record->bytes[run] == 0xffu) return 1;
            if (new_record->size - run < 3u) return 0;
            run += 3u;
            new_base = (uint16_t)(new_base + 0x0140u);
        }
    }

    while (run < new_record->size && new_record->bytes[run] != 0xffu) {
        uint32_t at;
        uint32_t length;
        uint32_t aligned;
        uint32_t words;
        if (new_record->size - run < 3u) return 0;
        if (direction == 0) {
            at = (uint16_t)(new_base + 0x2800u - new_record->bytes[run]);
            length = (uint32_t)new_record->bytes[run + 1u] + (at & 1u);
        }
        else {
            at = (uint16_t)(new_base + 0x2800u - 1u +
                new_record->bytes[run]);
            length = (uint32_t)new_record->bytes[run + 1u] - (at & 1u) + 1u;
        }
        aligned = at & ~UINT32_C(1);
        words = (length >> 1u) + (length & 1u);
        if (direction != 0 && words != 0) {
            /* STD makes REP MOVSW copy the first word at `aligned`, then
               continue toward lower addresses. */
            aligned -= (words - 1u) * 2u;
        }
        copy_word_aligned_span(
            road_buffer, background, aligned, words * 2u);
        new_base = (uint16_t)(new_base + 0x0140u);
        run += 3u;
    }
    return run < new_record->size;
}

static void restore_masked_pixels(
    uint8_t *road_buffer,
    const uint8_t *background,
    uint16_t screen_offset,
    const uint8_t *mask,
    unsigned rows) {
    unsigned row;
    unsigned column;
    for (row = 0; row < rows; ++row) {
        for (column = 0; column < SR_SHIP_MASK_WIDTH; ++column) {
            uint32_t at = (uint16_t)(screen_offset +
                row * SR_VGA_WIDTH + column);
            if (mask[row * SR_SHIP_MASK_WIDTH + column] == 2 &&
                at < SR_VGA_FRAMEBUFFER_SIZE) {
                road_buffer[at] = background[at];
            }
        }
    }
}

/* 1000:3A06: copy mask value 2 from the persistent road work buffer to the
   presented framebuffer.  The DOS screen pass applies both the previous and
   current car/shadow masks, including pixels above the 0x2800 road viewport. */
static void copy_masked_pixels(
    uint8_t *destination,
    const uint8_t *source,
    uint16_t screen_offset,
    const uint8_t *mask,
    unsigned rows) {
    unsigned row;
    unsigned column;
    for (row = 0; row < rows; ++row) {
        for (column = 0; column < SR_SHIP_MASK_WIDTH; ++column) {
            uint32_t at = (uint16_t)(screen_offset +
                row * SR_VGA_WIDTH + column);
            if (mask[row * SR_SHIP_MASK_WIDTH + column] == 2 &&
                at < SR_VGA_FRAMEBUFFER_SIZE) {
                destination[at] = source[at];
            }
        }
    }
}

static int prepare_road_buffer(
    SrTrekArchive *trek,
    const uint16_t *cells,
    size_t row_count,
    const SrRoadFrameParams *params,
    SrVgaRendererState *state,
    const uint8_t *background) {
    uint16_t current_phase = params->road_phase;
    uint16_t previous_phase = state->previous_road_phase;
    uint16_t phase_delta = (uint16_t)(current_phase - previous_phase);

    if (!state->road_buffer_initialized || phase_delta >= 8u) {
        memcpy(state->road_buffer + VGA_ROAD_VIEWPORT_OFFSET,
            background + VGA_ROAD_VIEWPORT_OFFSET,
            VGA_ROAD_VIEWPORT_END - VGA_ROAD_VIEWPORT_OFFSET);
        state->road_buffer_initialized = 1;
    }
    else if (current_phase != previous_phase) {
        const SrTrekRecord *old_record =
            &trek->records[previous_phase & 7u];
        const SrTrekRecord *new_record =
            &trek->records[current_phase & 7u];
        size_t phase_pointer_offset =
            (current_phase >> 3) == (previous_phase >> 3) ? 0x30u : 0u;
        int row = (int)(current_phase >> 3) + 7;
        int depth;
        for (depth = 11; depth != 1; --depth, --row) {
            unsigned slot;
            for (slot = 1; slot < 5; ++slot) {
                unsigned direction;
                for (direction = 0; direction < 2; ++direction) {
                    int column = direction == 0
                        ? 4 - (int)slot : (int)slot + 2;
                    uint16_t current;
                    uint16_t above;
                    uint16_t adjacent;
                    unsigned current_class;
                    unsigned above_class;
                    unsigned adjacent_class;
                    unsigned index;
                    static const uint16_t type_one_shapes[6] = {
                        0x0400, 0x0401, 0x0402, 0x0403, 0x0404, 0x0405
                    };
                    current = row < 0 || column < 0 || column >= 7 ||
                            (size_t)row >= row_count
                        ? 0 : cells[(size_t)row * 7u + (size_t)column];
                    above = row - 1 < 0 || column < 0 || column >= 7 ||
                            (size_t)(row - 1) >= row_count
                        ? 0 : cells[(size_t)(row - 1) * 7u + (size_t)column];
                    adjacent = row < 0 ||
                            column + (direction == 0 ? 1 : -1) < 0 ||
                            column + (direction == 0 ? 1 : -1) >= 7 ||
                            (size_t)row >= row_count
                        ? 0 : cells[(size_t)row * 7u + (size_t)(
                            column + (direction == 0 ? 1 : -1))];
                    current_class = road_delta_class(current);
                    above_class = road_delta_class(above);
                    adjacent_class = road_delta_class(adjacent);

#define TRANSFER(code_) do { \
    if (!transfer_background_shape(old_record, new_record, (code_), depth, \
            slot, direction, phase_pointer_offset, state->road_buffer, \
            background)) return 0; \
} while (0)
                    if ((current & 0x0fu) == 0 && (above & 0x0fu) != 0) {
                        if (above_class == 1) TRANSFER(0x0000);
                        if ((adjacent & 0x0fu) == 0) TRANSFER(0x0001);
                    }
                    if (current_class < 3) {
                        if (above_class == 3) TRANSFER(0x0200);
                        if (above_class >= 3 && adjacent_class < 3) {
                            TRANSFER(0x0201);
                        }
                    }
                    if (current_class == 1 && above_class == 2) {
                        for (index = 0; index < 6; ++index) {
                            TRANSFER(type_one_shapes[index]);
                        }
                    }
                    if (current_class < 4 && above_class == 4) {
                        TRANSFER(0x0500);
                        if (adjacent_class < 4) TRANSFER(0x0501);
                    }
#undef TRANSFER
                }
            }
        }
    }

    if (state->road_buffer_initialized) {
        restore_masked_pixels(state->road_buffer, background,
            state->previous_car_offset, state->previous_ship_mask,
            SR_SHIP_IMAGE_HEIGHT);
        restore_masked_pixels(state->road_buffer, background,
            state->previous_shadow_offset,
            state->previous_ship_mask +
                SR_SHIP_IMAGE_HEIGHT * SR_SHIP_MASK_WIDTH,
            SR_SHIP_SHADOW_HEIGHT);
    }
    return 1;
}

static int pointer_at(DrawContext *context, size_t relative, size_t *value) {
    size_t offset = context->pointer_base + relative;
    if (offset + 2u > 0x270u || offset + 2u > context->record->size) {
        context->state->failure_detail = 1;
        context->state->failure_offset = (uint32_t)offset;
        return 0;
    }
    *value = read_u16(context->record->bytes + offset);
    if (*value >= context->record->size) {
        context->state->failure_detail = 2;
        context->state->failure_offset = (uint32_t)*value;
        return 0;
    }
    context->pointer_relative = (unsigned)relative;
    context->shape_ordinal = 0u;
    return 1;
}

static int set_shape_color(DrawContext *context, size_t shape, uint8_t color) {
    if (shape >= context->record->size || color >= 74) {
        context->state->failure_detail = color >= 74 ? 4 : 3;
        context->state->failure_offset = (uint32_t)shape;
        return 0;
    }
    context->record->bytes[shape] = color;
    return 1;
}

static void report_shape_span(
    DrawContext *context,
    uint16_t base,
    uint8_t start,
    unsigned length,
    unsigned direction) {
    int scanline_origin;
    int y;
    int anchor_x;
    int left;
    int right;
    if (context->state->geometry_hooks.span == 0 || length == 0) return;
    scanline_origin = 0x2800 + (int)base;
    y = scanline_origin / SR_VGA_WIDTH;
    anchor_x = scanline_origin % SR_VGA_WIDTH;
    if (direction == 0) {
        left = anchor_x - (int)start;
        right = left + (int)length;
    }
    else {
        right = anchor_x - 1 + (int)start + 1;
        left = right - (int)length;
    }
    context->state->geometry_hooks.span(
        context->state->geometry_hooks.context,
        (int16_t)y, (int16_t)left, (int16_t)right);
}

static int draw_shape(DrawContext *context, size_t *shape) {
    uint8_t *bytes = context->record->bytes;
    size_t cursor = *shape;
    uint8_t color_index;
    uint8_t color;
    uint16_t base;

    if (cursor + 4u > context->record->size) {
        context->state->failure_detail = 5;
        context->state->failure_offset = (uint32_t)cursor;
        return 0;
    }
    color_index = bytes[cursor++];
    if (color_index >= 74) {
        context->state->failure_detail = 6;
        context->state->failure_offset = (uint32_t)(cursor - 1u);
        return 0;
    }
    color = color_mapping[color_index * 4u + context->direction];
    base = read_u16(bytes + cursor);
    cursor += 2;
    context->state->geometry_hooks.current_row = (int16_t)context->row;
    context->state->geometry_hooks.current_column = (int16_t)context->column;
    context->state->geometry_hooks.current_pointer_base =
        (uint16_t)context->pointer_base;
    context->state->geometry_hooks.current_shape_offset = (uint16_t)*shape;
    context->state->geometry_hooks.current_pointer_relative =
        (uint8_t)context->pointer_relative;
    context->state->geometry_hooks.current_shape_ordinal =
        (uint8_t)context->shape_ordinal;
    context->state->geometry_hooks.current_direction =
        (uint8_t)context->direction;
    if (context->state->geometry_hooks.begin_shape != 0) {
        context->state->geometry_hooks.begin_shape(
            context->state->geometry_hooks.context, color);
    }
    for (;;) {
        uint8_t start;
        uint8_t length;
        int destination;
        unsigned pixel;
        if (cursor >= context->record->size) {
            context->state->failure_detail = 7;
            context->state->failure_offset = (uint32_t)cursor;
            return 0;
        }
        start = bytes[cursor];
        if (start == 0xffu) {
            /* LODSB consumes the terminator before the DOS primitive returns. */
            *shape = cursor + 1u;
            if (context->state->geometry_hooks.end_shape != 0) {
                context->state->geometry_hooks.end_shape(
                    context->state->geometry_hooks.context);
            }
            ++context->shape_ordinal;
            return 1;
        }
        if (cursor + 3u > context->record->size) {
            context->state->failure_detail = 8;
            context->state->failure_offset = (uint32_t)cursor;
            return 0;
        }
        length = bytes[cursor + 1u];
        destination = 0x2800 + base;
        if (context->direction == 0) {
            destination -= start;
            report_shape_span(
                context, base, start, length, context->direction);
            for (pixel = 0; pixel < length; ++pixel) {
                int at = destination + (int)pixel;
                if (at >= 0 && at < SR_VGA_FRAMEBUFFER_SIZE) {
                    context->framebuffer[at] = color;
                }
            }
        }
        else {
            destination = destination - 1 + start;
            report_shape_span(
                context, base, start, length, context->direction);
            for (pixel = 0; pixel < length; ++pixel) {
                int at = destination - (int)pixel;
                if (at >= 0 && at < SR_VGA_FRAMEBUFFER_SIZE) {
                    context->framebuffer[at] = color;
                }
            }
        }
        base = (uint16_t)(base + 0x140u);
        cursor += 3u;
    }
}

static int skip_shape(DrawContext *context, size_t *shape) {
    size_t cursor = *shape;
    do {
        cursor += 3u;
        if (cursor >= context->record->size) {
            context->state->failure_detail = 9;
            context->state->failure_offset = (uint32_t)cursor;
            return 0;
        }
    } while (context->record->bytes[cursor] != 0xffu);
    /* 31BD increments SI past the terminator. */
    *shape = cursor + 1u;
    ++context->shape_ordinal;
    return 1;
}

static int draw_type_0(DrawContext *context, uint16_t cell, size_t *shape) {
    uint8_t color = (uint8_t)(cell & 0x000fu);
    int adjacent = context->direction == 0 ? 1 : -1;
    if (color == 0) return 1;
    if (!pointer_at(context, 0, shape) ||
        !set_shape_color(context, *shape, color) || !draw_shape(context, shape)) return 0;
    if ((road_cell(context, context->row, context->column + adjacent) & 0x000fu) == 0) {
        if (!set_shape_color(context, *shape, (uint8_t)(color + 0x1eu)) ||
            !draw_shape(context, shape)) return 0;
    }
    else if (!skip_shape(context, shape)) return 0;
    if ((road_cell(context, context->row - 1, context->column) & 0x000fu) == 0) {
        if (!set_shape_color(context, *shape, (uint8_t)(color + 0x0fu)) ||
            !draw_shape(context, shape)) return 0;
    }
    return 1;
}

static int draw_cell(DrawContext *context) {
    uint16_t cell = road_cell(context, context->row, context->column);
    unsigned type = (cell >> 8) & 0x0fu;
    unsigned above_class = road_type(
        road_cell(context, context->row - 1, context->column));
    int adjacent = context->direction == 0 ? 1 : -1;
    unsigned adjacent_class = road_type(
        road_cell(context, context->row, context->column + adjacent));
    size_t shape = 0;
    unsigned count;
    uint8_t color;

    context->state->geometry_hooks.current_cell = cell;

    if (type > 5) return 1;
    if (!draw_type_0(context, cell, &shape)) return 0;
    if (type == 0) return 1;
    if (type == 1) {
        if (above_class < 1) {
            if (!pointer_at(context, 2, &shape) || !set_shape_color(context, shape, 0x43) ||
                !draw_shape(context, &shape)) return 0;
        }
        if (!pointer_at(context, 8, &shape)) return 0;
        for (count = 0; count < 6; ++count) if (!draw_shape(context, &shape)) return 0;
        if (above_class < 1) {
            if (!draw_shape(context, &shape) || !draw_shape(context, &shape)) return 0;
        }
        return 1;
    }
    if (type == 2) {
        if (above_class < 2) {
            if (!pointer_at(context, 6, &shape) || !draw_shape(context, &shape)) return 0;
        }
        if (!pointer_at(context, 4, &shape)) return 0;
        color = (uint8_t)((uint8_t)cell >> 4);
        if (color == 0) color = 0x3d;
        if (!set_shape_color(context, shape, color) || !draw_shape(context, &shape)) return 0;
        if (adjacent_class < 2 && !draw_shape(context, &shape)) return 0;
        return 1;
    }
    if (type == 3) {
        if (above_class < 2) {
            if (!pointer_at(context, 2, &shape) || !set_shape_color(context, shape, 0x41) ||
                !draw_shape(context, &shape)) return 0;
        }
        if (!pointer_at(context, 4, &shape)) return 0;
        /* The DOS code shifts AL, not AX: the road type byte must not leak
         * into the dynamic VGA color index. */
        color = (uint8_t)((uint8_t)cell >> 4);
        if (color == 0) color = 0x3d;
        if (!set_shape_color(context, shape, color) || !draw_shape(context, &shape)) return 0;
        if (adjacent_class < 2 && !draw_shape(context, &shape)) return 0;
        if (above_class < 2) {
            if (!pointer_at(context, 6, &shape) ||
                !skip_shape(context, &shape) || !draw_shape(context, &shape) ||
                !draw_shape(context, &shape)) return 0;
        }
        return 1;
    }

    if (above_class < 2) {
        size_t relative = type == 5 ? 2u : 6u;
        if (!pointer_at(context, relative, &shape)) return 0;
        if (type == 5 && !set_shape_color(context, shape, 0x41)) return 0;
        if (!draw_shape(context, &shape)) return 0;
    }
    if (!pointer_at(context, 4, &shape) || !skip_shape(context, &shape)) return 0;
    if (adjacent_class < 2 && !draw_shape(context, &shape)) return 0;
    if (type == 5 && above_class < 2) {
        if (!pointer_at(context, 6, &shape) || !skip_shape(context, &shape) ||
            !draw_shape(context, &shape) || !draw_shape(context, &shape)) return 0;
    }
    if (!pointer_at(context, 10, &shape)) return 0;
    color = (uint8_t)((uint8_t)cell >> 4);
    if (color == 0) color = 0x3d;
    if (!set_shape_color(context, shape, color) || !draw_shape(context, &shape)) return 0;
    if (adjacent_class < 4) {
        if (!draw_shape(context, &shape)) return 0;
    }
    else if (!skip_shape(context, &shape)) return 0;
    if (above_class < 4 && !draw_shape(context, &shape)) return 0;
    return 1;
}

static int draw_depth(DrawContext *context, size_t pointer_row) {
    static const int columns[2][4] = {{0, 1, 2, 3}, {6, 5, 4, 3}};
    unsigned direction;
    unsigned index;
    context->pointer_base = pointer_row * 0x30u;
    for (direction = 0; direction < 2; ++direction) {
        context->direction = direction;
        for (index = 0; index < 4; ++index) {
            context->column = columns[direction][index];
            context->pointer_base = pointer_row * 0x30u + index * 0x0cu;
            if (!draw_cell(context)) return 0;
        }
    }
    return 1;
}

static int build_ship_mask(
    const SrRoadFrameParams *params,
    const SrRendererTables *tables,
    SrVgaRendererState *state) {
    uint16_t height = params->ship_height_units;
    uint16_t table_index = (uint16_t)(0x009du - height);
    unsigned remaining = SR_SHIP_MASK_HEIGHT;
    unsigned row = 0;

    /* 32A5 clears 0x1DE words: all but byte 956 of the 957-byte mask. */
    memset(state->ship_mask, 0, SR_SHIP_MASK_SIZE - 1u);
    while (remaining != 0) {
        if (height <= 0x009du && height >= 0x0014u) {
            int16_t horizontal = (int16_t)params->horizontal_sample;
            int16_t lower = 0x006e;
            int16_t upper = 0x01ae;
            uint16_t half_width;
            int16_t start;
            int16_t end_cut;
            uint16_t count;

            if (table_index >= SR_VISIBILITY_HEIGHT_COUNT) return 0;
            half_width = tables->visibility_half_widths[table_index];
            if (half_width != 0) {
                int16_t left = (int16_t)(0x010eu - half_width);
                if (horizontal >= left) {
                    lower = (int16_t)(0x010eu + half_width);
                }
                if (horizontal < left) upper = left;
            }
            start = (int16_t)(lower - horizontal);
            if (start < 0) start = 0;
            if ((uint16_t)start < SR_SHIP_MASK_WIDTH) {
                end_cut = (int16_t)(horizontal + SR_SHIP_MASK_WIDTH - upper);
                if (end_cut < 0) end_cut = 0;
                count = (uint16_t)(SR_SHIP_MASK_WIDTH - (uint16_t)start);
                if ((uint16_t)end_cut < count) {
                    memset(state->ship_mask + row * SR_SHIP_MASK_WIDTH +
                            (uint16_t)start,
                        1, (size_t)(count - (uint16_t)end_cut));
                }
            }
        }
        --height;
        ++table_index;
        if (remaining == 10u) {
            uint16_t adjustment =
                (uint16_t)(params->surface_clearance_units - 8u);
            height = (uint16_t)(height - adjustment);
            table_index = (uint16_t)(table_index + adjustment);
        }
        --remaining;
        ++row;
    }
    return 1;
}

static int draw_car_and_shadow(
    uint8_t *framebuffer,
    const SrRoadFrameParams *params,
    const SrCarSprites *cars,
    const SrRendererTables *tables,
    SrVgaRendererState *state) {
    const uint8_t *source;
    int base_x;
    int base_y;
    unsigned x;
    unsigned y;
    if (params->ship_frame == 0xffffu) return 1;
    if (cars == 0 || tables == 0 || state == 0 ||
        params->ship_frame >= cars->frame_count ||
        params->ship_frame_byte_offset + 0x02d0u > cars->pixel_count) return 0;
    if (!build_ship_mask(params, tables, state)) return 0;
    source = cars->pixels + params->ship_frame_byte_offset;
    base_x = (int)params->horizontal_sample - 0x6e;
    base_y = 0x9d - (int)params->ship_height_units;
    for (x = 0; x < 0x1d; ++x) {
        for (y = 0; y < 0x18; ++y) {
            uint8_t pixel = source[x * 0x18u + y];
            int screen_x = base_x + (int)x;
            int screen_y = base_y + (int)y;
            size_t mask_at = y * SR_SHIP_MASK_WIDTH + x;
            if (pixel != 0 && state->ship_mask[mask_at] != 0) {
                state->ship_mask[mask_at] = 2;
                if (screen_x >= 0 && screen_x < SR_VGA_WIDTH &&
                    screen_y >= 0 && screen_y < SR_VGA_HEIGHT) {
                    framebuffer[screen_y * SR_VGA_WIDTH + screen_x] = pixel;
                }
            }
        }
    }

    {
        unsigned shadow_frame = params->surface_clearance_units / 5u;
        if (shadow_frame < SR_SHADOW_FRAME_COUNT) {
            base_y = 0x9d - (int)params->ship_height_units + 0x10 +
                (int)params->surface_clearance_units;
            for (x = 0; x < SR_SHIP_MASK_WIDTH; ++x) {
                for (y = 0; y < SR_SHIP_SHADOW_HEIGHT; ++y) {
                    size_t shadow_at = y * SR_SHIP_MASK_WIDTH + x;
                    size_t mask_at = SR_SHIP_IMAGE_HEIGHT * SR_SHIP_MASK_WIDTH +
                        shadow_at;
                    int screen_x = base_x + (int)x;
                    int screen_y = base_y + (int)y;
                    if (tables->shadows[shadow_frame][shadow_at] != 0 &&
                        state->ship_mask[mask_at] != 0) {
                        state->ship_mask[mask_at] = 2;
                        if (screen_x >= 0 && screen_x < SR_VGA_WIDTH &&
                            screen_y >= 0 && screen_y < SR_VGA_HEIGHT) {
                            uint8_t pixel =
                                framebuffer[screen_y * SR_VGA_WIDTH + screen_x];
                            if (pixel == 0x3du) pixel = 0x40u;
                            if (pixel != 0 && pixel < 0x10u) {
                                pixel = (uint8_t)(pixel + 0x2du);
                            }
                            framebuffer[screen_y * SR_VGA_WIDTH + screen_x] = pixel;
                        }
                    }
                }
            }
        }
    }
    return 1;
}

int sr_draw_road_scene_vga(
    SrTrekArchive *trek,
    const uint16_t *cells,
    size_t row_count,
    const SrRoadFrameParams *params,
    const SrCarSprites *cars,
    const SrRendererTables *tables,
    SrVgaRendererState *state,
    const uint8_t background[SR_VGA_FRAMEBUFFER_SIZE],
    uint8_t framebuffer[SR_VGA_FRAMEBUFFER_SIZE]) {
    DrawContext context;
    int depth;
    size_t pointer_row = 0;
    if (trek == 0 || params == 0 || background == 0 || framebuffer == 0 ||
        cells == 0 || state == 0 ||
        trek->record_count != 8) return 0;
    state->failure_stage = 0;
    state->failure_depth = 0;
    state->failure_row = 0;
    state->failure_column = 0;
    state->failure_detail = 0;
    state->failure_offset = 0;
    if (!prepare_road_buffer(
            trek, cells, row_count, params, state, background)) {
        state->failure_stage = 5;
        return 0;
    }
    context.record = &trek->records[params->road_phase & 7u];
    context.cells = cells;
    context.row_count = row_count;
    context.framebuffer = state->road_buffer;
    context.row = (int)(params->road_phase >> 3) + 7;
    context.column = 0;
    context.direction = 0;
    context.pointer_base = 0;
    context.pointer_relative = 0;
    context.shape_ordinal = 0;
    context.state = state;

    for (depth = 0x0b; depth >= 1; --depth) {
        if (depth == 4) {
            if (!draw_depth(&context, 11)) {
                state->failure_stage = 1;
                goto failed;
            }
            if (state->geometry_hooks.ship_layer != 0) {
                state->geometry_hooks.ship_layer(
                    state->geometry_hooks.context);
            }
            if (!draw_car_and_shadow(
                    state->road_buffer, params, cars, tables, state)) {
                state->failure_stage = 2;
                goto failed;
            }
            if (!draw_depth(&context, 12)) {
                state->failure_stage = 3;
                goto failed;
            }
        }
        else {
            if (!draw_depth(&context, pointer_row)) {
                state->failure_stage = 4;
                goto failed;
            }
        }
        ++pointer_row;
        --context.row;
    }
    memcpy(framebuffer + VGA_ROAD_VIEWPORT_OFFSET,
        state->road_buffer + VGA_ROAD_VIEWPORT_OFFSET,
        VGA_ROAD_VIEWPORT_END - VGA_ROAD_VIEWPORT_OFFSET);
    copy_masked_pixels(framebuffer, state->road_buffer,
        state->previous_car_offset, state->previous_ship_mask,
        SR_SHIP_IMAGE_HEIGHT);
    copy_masked_pixels(framebuffer, state->road_buffer,
        state->previous_shadow_offset,
        state->previous_ship_mask +
            SR_SHIP_IMAGE_HEIGHT * SR_SHIP_MASK_WIDTH,
        SR_SHIP_SHADOW_HEIGHT);
    {
        uint16_t current_car_offset = (uint16_t)(
            (0x9d - (int)params->ship_height_units) * SR_VGA_WIDTH +
            (int)params->horizontal_sample - 0x6e);
        uint16_t current_shadow_offset = (uint16_t)(
            (0x9d - (int)params->ship_height_units + 0x10 +
                (int)params->surface_clearance_units) * SR_VGA_WIDTH +
            (int)params->horizontal_sample - 0x6e);
        copy_masked_pixels(framebuffer, state->road_buffer,
            current_car_offset, state->ship_mask, SR_SHIP_IMAGE_HEIGHT);
        copy_masked_pixels(framebuffer, state->road_buffer,
            current_shadow_offset,
            state->ship_mask +
                SR_SHIP_IMAGE_HEIGHT * SR_SHIP_MASK_WIDTH,
            SR_SHIP_SHADOW_HEIGHT);
    }
    state->previous_road_phase = params->road_phase;
    state->previous_car_offset = (uint16_t)(
        (0x9d - (int)params->ship_height_units) * SR_VGA_WIDTH +
        (int)params->horizontal_sample - 0x6e);
    state->previous_shadow_offset = (uint16_t)(
        (0x9d - (int)params->ship_height_units + 0x10 +
            (int)params->surface_clearance_units) * SR_VGA_WIDTH +
        (int)params->horizontal_sample - 0x6e);
    memcpy(state->previous_ship_mask, state->ship_mask,
        sizeof(state->previous_ship_mask));
    return 1;

failed:
    state->failure_depth = (int16_t)depth;
    state->failure_row = (int16_t)context.row;
    state->failure_column = (int16_t)context.column;
    return 0;
}

int sr_restore_road_viewport_vga(
    uint8_t framebuffer[SR_VGA_FRAMEBUFFER_SIZE],
    const uint8_t background[SR_VGA_FRAMEBUFFER_SIZE]) {
    if (framebuffer == 0 || background == 0) return 0;
    memcpy(framebuffer + VGA_ROAD_VIEWPORT_OFFSET,
        background + VGA_ROAD_VIEWPORT_OFFSET,
        VGA_ROAD_VIEWPORT_END - VGA_ROAD_VIEWPORT_OFFSET);
    return 1;
}
