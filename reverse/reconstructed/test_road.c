#include "road.h"

#include <stdio.h>

int main(void) {
    static const uint16_t cells[14] = {
        10, 11, 12, 13, 14, 15, 16,
        20, 21, 22, 23, 24, 25, 26
    };
    unsigned column;

    for (column = 0; column < 7; ++column) {
        uint16_t x = (uint16_t)((0x5f + column * 0x2e) * 0x80);
        if (sr_road_cell_at_position(cells, 2, 0, x) != (uint16_t)(10 + column) ||
            sr_road_cell_at_position(cells, 2, UINT32_C(0x10000), x) !=
                (uint16_t)(20 + column)) {
            fputs("road lookup column vector failed\n", stderr);
            return 1;
        }
    }
    if (sr_road_cell_at_position(cells, 2, 0, 0) != 0 ||
        sr_road_cell_at_position(cells, 2, UINT32_C(0x20000), 0x8000) != 0) {
        fputs("road lookup boundary vector failed\n", stderr);
        return 1;
    }
    {
        uint16_t collision_cells[7] = {0, 0, 0x0200, 0x0200, 0x0200, 0, 0};
        if (!sr_test_ship_collision(collision_cells, 1, 0, 0x8000, 0x3000) ||
            sr_test_ship_collision(collision_cells, 1, 0, 0x8000, 0x3300)) {
            fputs("road obstacle-height vector failed\n", stderr);
            return 1;
        }
        collision_cells[2] = 1;
        collision_cells[3] = 1;
        collision_cells[4] = 1;
        if (!sr_test_ship_collision(collision_cells, 1, 0, 0x8000, 0x2500)) {
            fputs("road wing-floor vector failed\n", stderr);
            return 1;
        }
    }
    {
        uint16_t empty[14] = {0};
        SrShipPosition position = {0, 0x8000, 0x2800};
        SrShipPosition target = {UINT32_C(0x10000), 0x8200, 0x2a00};
        sr_move_ship_with_collision(empty, 2, &position, target);
        if (position.distance != target.distance ||
            position.horizontal_position != target.horizontal_position ||
            position.height != target.height) {
            fputs("collision-free movement vector failed\n", stderr);
            return 1;
        }
    }
    {
        uint16_t barrier[14] = {0};
        SrShipPosition position = {0, 0x8000, 0x3000};
        SrShipPosition target = {UINT32_C(0x10000), 0x8000, 0x3000};
        unsigned index;
        for (index = 7; index < 14; ++index) barrier[index] = 0x0200;
        sr_move_ship_with_collision(barrier, 2, &position, target);
        if (position.distance >= target.distance ||
            sr_test_ship_collision(
                barrier, 2, position.distance, position.horizontal_position, position.height)) {
            fputs("collision-constrained movement vector failed\n", stderr);
            return 1;
        }
    }
    puts("Recovered road-cell lookup passed coordinate and boundary vectors");
    return 0;
}
