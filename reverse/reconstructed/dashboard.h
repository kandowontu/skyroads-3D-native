#ifndef SKYROADS_RECOVERED_DASHBOARD_H
#define SKYROADS_RECOVERED_DASHBOARD_H

#include <stdint.h>

#include "display_table.h"
#include "embedded_hud.h"
#include "graphics_archive.h"
#include "picture_blitter.h"

typedef struct SrDashboardState {
    uint16_t previous_speed_level;      /* DS:41CA */
    uint16_t previous_oxygen_level;     /* DS:457A */
    uint16_t previous_fuel_level;       /* DS:9618 */
    uint16_t previous_progress_column;  /* DS:456A */
    uint16_t previous_jumpmaster;       /* DS:AF30 */
    uint16_t previous_warning_phase;    /* DS:AF2E */
} SrDashboardState;

typedef struct SrDashboardInput {
    uint16_t tick_count;
    int32_t forward_speed;
    int32_t collision_speed_correction;
    uint16_t oxygen;
    uint16_t fuel;
    uint16_t level_result;
    uint16_t road_length_rows;
    uint16_t jumpmaster;
    uint32_t distance;
} SrDashboardInput;

typedef struct SrDashboardHooks {
    void *context;
    void (*play_sound)(void *context, unsigned effect);
} SrDashboardHooks;

void sr_dashboard_state_init(SrDashboardState *state);

/* VGA implementations of 0EB5/0EDF and the direct picture path used to build
   the 320x200 world/dashboard background. */
int sr_render_dashboard_sprite_vga(
    uint8_t framebuffer[SR_PICTURE_SCREEN_SIZE],
    const SrDisplaySprite *sprite,
    int alternate_colors);
int sr_build_gameplay_background_vga(
    uint8_t framebuffer[SR_PICTURE_SCREEN_SIZE],
    const SrPicture *world,
    const SrPicture *dashboard);

/* Static gravity readout drawn by 1067 from EXE-resident digit bitmaps. */
int sr_draw_dashboard_gravity_vga(
    uint8_t framebuffer[SR_PICTURE_SCREEN_SIZE],
    const SrEmbeddedHud *hud,
    uint16_t road_gravity);

/* Complete VGA semantic translation of update_gameplay_dashboard 124B. */
int sr_update_gameplay_dashboard_vga(
    uint8_t framebuffer[SR_PICTURE_SCREEN_SIZE],
    const SrDisplayTable *speed,
    const SrDisplayTable *oxygen,
    const SrDisplayTable *fuel,
    const SrEmbeddedHud *hud,
    const SrDashboardInput *input,
    const SrDashboardHooks *hooks,
    SrDashboardState *state);

#endif
