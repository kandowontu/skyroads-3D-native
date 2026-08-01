#include "motion.h"

#include <stdio.h>
#include <string.h>

int main(void) {
    uint16_t empty[7 * 64];
    uint16_t floor_cells[7 * 64];
    SrMotionState motion;
    SrMotionState original;
    unsigned index;

    memset(empty, 0, sizeof(empty));
    for (index = 0; index < 7 * 64; ++index) {
        floor_cells[index] = 1;
    }

    memset(&motion, 0, sizeof(motion));
    motion.position.horizontal_position = 0x8000;
    motion.position.height = 0x3000;
    motion.vertical_velocity = -0x100;
    motion.gravity_step = -0x10;
    if (sr_predict_collision_motion(empty, 64, &motion) != 0 ||
        sr_predict_collision_motion(floor_cells, 64, &motion) != 1) {
        fputs("collision prediction vectors failed\n", stderr);
        return 1;
    }

    original = motion;
    sr_resolve_collision_motion(floor_cells, 64, &motion);
    if (memcmp(&motion, &original, sizeof(motion)) != 0) {
        fputs("already-supported collision resolution changed state\n", stderr);
        return 1;
    }

    memset(&motion, 0, sizeof(motion));
    motion.position.horizontal_position = 0x8000;
    motion.position.height = 0x3000;
    motion.forward_speed = 1000;
    motion.lateral_velocity = 100;
    motion.vertical_velocity = -0x100;
    motion.gravity_step = -0x10;
    sr_resolve_collision_motion(empty, 64, &motion);
    if (motion.forward_speed != 1000 || motion.lateral_velocity != 100 ||
        motion.collision_speed_correction != 0 || motion.collision_adjusted != 0) {
        fputs("unsolved collision search did not restore original motion\n", stderr);
        return 1;
    }

    puts("Recovered landing predictor and collision search passed control-flow vectors");
    return 0;
}
