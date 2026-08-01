#include "road.h"

static const uint16_t collision_lower[38] = {
    0x10, 0x10, 0x10, 0x10, 0x0f, 0x0e, 0x0d, 0x0b,
    0x08, 0x07, 0x06, 0x05, 0x03, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x02, 0x03, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x05, 0x06, 0x07, 0x08
};

static const uint16_t collision_upper[38] = {
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1e, 0x1e,
    0x1e, 0x1d, 0x1d, 0x1d, 0x1c, 0x1b, 0x1a, 0x19,
    0x18, 0x16, 0x14, 0x12, 0x11, 0x0e
};

static int cell_blocks_profile(uint16_t cell, uint16_t profile, uint16_t height) {
    uint16_t height_index;
    uint16_t shape;

    if (profile >= 38) {
        return 0;
    }
    height_index = (uint16_t)(height + 0xde00u) / 0x80u;
    shape = cell & 0x0f00u;
    if (shape == 0x0100u) {
        return height_index < collision_upper[profile] &&
            height_index >= collision_lower[profile];
    }
    if (shape == 0x0200u) {
        return height < 0x3200u;
    }
    if (shape == 0x0300u) {
        return height < 0x3200u && height_index >= collision_lower[profile];
    }
    if (shape == 0x0400u) {
        return height < 0x3c00u;
    }
    if (shape == 0x0500u) {
        return height < 0x3c00u && height_index >= collision_lower[profile];
    }
    return 0;
}

uint16_t sr_road_cell_at_position(
    const uint16_t *cells,
    size_t row_count,
    uint32_t distance,
    uint16_t horizontal_position) {
    uint16_t horizontal_cell = (uint16_t)(horizontal_position / 0x80u - 0x5fu);
    size_t row;
    size_t column;

    if (horizontal_cell >= 0x142u) {
        return 0;
    }
    row = (size_t)(distance >> 16);
    column = horizontal_cell / 0x2eu;
    if (row >= row_count) {
        return 0;
    }
    return cells[row * 7 + column];
}

int sr_test_ship_collision(
    const uint16_t *cells,
    size_t row_count,
    uint32_t distance,
    uint16_t horizontal_position,
    uint16_t ship_height) {
    uint16_t positive_wing = sr_road_cell_at_position(
        cells, row_count, distance, (uint16_t)(horizontal_position + 0x700u));
    uint16_t negative_wing = sr_road_cell_at_position(
        cells, row_count, distance, (uint16_t)(horizontal_position - 0x700u));
    uint16_t raised_height = (uint16_t)(ship_height + 0x600u);
    uint16_t clearance_height = (uint16_t)(ship_height + 0x680u);
    uint16_t phase;
    int16_t adjacent_offset;
    uint16_t center_cell;

    if (((positive_wing & 0x000fu) != 0 || (negative_wing & 0x000fu) != 0) &&
        ship_height < 0x2800u && raised_height > 0x2480u) {
        return 1;
    }
    if (clearance_height < 0x2801u ||
        (((negative_wing & 0x0f00u) == 0) && ((positive_wing & 0x0f00u) == 0))) {
        return 0;
    }

    center_cell = sr_road_cell_at_position(
        cells, row_count, distance, horizontal_position);
    phase = (uint16_t)(0x17u -
        (uint16_t)(((uint16_t)(horizontal_position / 0x80u - 0x31u)) % 0x2eu));
    adjacent_offset = -0x1700;
    if (phase > 0x7fffu || phase == 0) {
        phase = (uint16_t)(1u - phase);
        adjacent_offset = 0x1700;
    }
    if (cell_blocks_profile(center_cell, phase, ship_height)) {
        return 1;
    }

    center_cell = sr_road_cell_at_position(
        cells,
        row_count,
        distance,
        (uint16_t)(horizontal_position + adjacent_offset));
    return cell_blocks_profile(center_cell, (uint16_t)(0x2fu - phase), ship_height);
}

int sr_test_finish_gate(
    const uint16_t *cells,
    size_t row_count,
    uint32_t distance,
    uint16_t horizontal_position,
    uint16_t ship_height) {
    uint16_t cell = sr_road_cell_at_position(
        cells, row_count, distance, horizontal_position);
    uint16_t shape = cell & 0x0f00u;
    uint16_t phase;
    uint16_t height_index;
    uint16_t midpoint;

    if (shape != 0x0100u && shape != 0x0300u && shape != 0x0500u) {
        return 0;
    }
    phase = (uint16_t)(0x17u -
        (uint16_t)(((uint16_t)(horizontal_position / 0x80u - 0x31u)) % 0x2eu));
    if (phase > 0x7fffu || phase == 0) {
        phase = (uint16_t)(1u - phase);
    }
    if (phase >= 38) {
        return 0;
    }
    height_index = (uint16_t)(ship_height + 0xde00u) / 0x80u;
    midpoint = (uint16_t)((collision_lower[phase] + collision_upper[phase]) / 2u);
    return height_index < midpoint;
}

static int16_t signed_delta(uint16_t target, uint16_t current) {
    return (int16_t)(uint16_t)(target - current);
}

static int16_t absolute16(int16_t value) {
    return value < 0 ? (int16_t)-value : value;
}

void sr_move_ship_with_collision(
    const uint16_t *cells,
    size_t row_count,
    SrShipPosition *position,
    SrShipPosition target) {
    unsigned step;
    uint32_t distance_delta;
    int16_t horizontal_delta;
    int16_t height_delta;
    uint32_t distance_increment;
    int16_t increment;

    if (position->distance == target.distance &&
        position->horizontal_position == target.horizontal_position &&
        position->height == target.height) {
        return;
    }

    distance_delta = target.distance - position->distance;
    horizontal_delta = signed_delta(target.horizontal_position, position->horizontal_position);
    height_delta = signed_delta(target.height, position->height);
    for (step = 1; step <= 5; ++step) {
        SrShipPosition sample;
        sample.distance = position->distance +
            (uint32_t)(((uint64_t)distance_delta * step) / 5u);
        sample.horizontal_position = (uint16_t)(position->horizontal_position +
            (int32_t)horizontal_delta * (int)step / 5);
        sample.height = (uint16_t)(position->height +
            (int32_t)height_delta * (int)step / 5);
        if (sr_test_ship_collision(
                cells, row_count, sample.distance, sample.horizontal_position, sample.height)) {
            break;
        }
    }

    position->distance += (uint32_t)(((uint64_t)distance_delta * (step - 1u)) / 5u);
    position->horizontal_position = (uint16_t)(position->horizontal_position +
        (int32_t)horizontal_delta * (int)(step - 1u) / 5);
    position->height = (uint16_t)(position->height +
        (int32_t)height_delta * (int)(step - 1u) / 5);

    for (distance_increment = 0x1000u; distance_increment != 0; distance_increment /= 0x10u) {
        while (distance_increment <= target.distance - position->distance &&
            !sr_test_ship_collision(
                cells,
                row_count,
                position->distance + distance_increment,
                position->horizontal_position,
                position->height)) {
            position->distance += distance_increment;
        }
    }

    increment = position->horizontal_position < target.horizontal_position ? 0x7d : -0x7d;
    while (increment != 0) {
        while (absolute16(signed_delta(target.horizontal_position, position->horizontal_position)) >=
                absolute16(increment) &&
            !sr_test_ship_collision(
                cells,
                row_count,
                position->distance,
                (uint16_t)(position->horizontal_position + increment),
                position->height)) {
            position->horizontal_position = (uint16_t)(position->horizontal_position + increment);
        }
        increment = (int16_t)(increment / 5);
    }

    increment = position->height < target.height ? 0x7d : -0x7d;
    while (increment != 0) {
        while (absolute16(signed_delta(target.height, position->height)) >= absolute16(increment) &&
            !sr_test_ship_collision(
                cells,
                row_count,
                position->distance,
                position->horizontal_position,
                (uint16_t)(position->height + increment))) {
            position->height = (uint16_t)(position->height + increment);
        }
        increment = (int16_t)(increment / 5);
    }
}
