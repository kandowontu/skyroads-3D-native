#include "physics.h"

#include <stdio.h>

static void capture_sound(void *context, unsigned effect) {
    *(unsigned *)context = effect + 1;
}

int main(void) {
    int32_t speed = 0;
    unsigned tick;

    if (sr_gravity_step(8) != -115 || sr_gravity_step(20) != -288) {
        fputs("gravity constants failed\n", stderr);
        return 1;
    }
    for (tick = 0; tick < 200; ++tick) {
        speed = sr_adjust_forward_speed(speed, 1);
    }
    if (speed != 0x2aaa || sr_adjust_forward_speed(20, -1) != 0) {
        fputs("forward-speed clamp failed\n", stderr);
        return 1;
    }
    if (sr_apply_airborne_gravity(0x2800, 0, -115) != -115 ||
        sr_apply_airborne_gravity(0x2700, 0, -115) != -0x6a ||
        sr_apply_airborne_gravity(0x2700, -200, -115) != -200) {
        fputs("vertical-gravity branch failed\n", stderr);
        return 1;
    }
    if (sr_consume_oxygen(30000, 180) != 29996 ||
        sr_consume_fuel(30000, 150, 0x2aaa) != 29967) {
        fputs("resource-consumption constants failed\n", stderr);
        return 1;
    }
    {
        unsigned sound = 0;
        SrCellEffectState effect = {1000, 20000, 25000, 0, 0};
        sr_apply_cell_effect(&effect, 9, capture_sound, &sound);
        if (effect.fuel != 30000 || effect.oxygen != 30000 || sound != 5) {
            fputs("refill cell effect failed\n", stderr);
            return 1;
        }
        sr_apply_cell_effect(&effect, 10, capture_sound, &sound);
        if (effect.forward_speed != 1303) {
            fputs("boost cell effect failed\n", stderr);
            return 1;
        }
        sr_apply_cell_effect(&effect, 12, capture_sound, &sound);
        if (effect.level_result != 2 || effect.result_ticks != 1 || sound != 1) {
            fputs("end cell effect failed\n", stderr);
            return 1;
        }
    }
    puts("Recovered fixed-point physics scalars passed executable vectors");
    return 0;
}
