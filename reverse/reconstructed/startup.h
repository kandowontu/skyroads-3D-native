#ifndef SKYROADS_RECOVERED_STARTUP_H
#define SKYROADS_RECOVERED_STARTUP_H

#include <stdint.h>

typedef struct SrRecoveredGlobals {
    uint16_t video_is_vga;              /* DS:0036 */
    uint16_t io_error;                  /* DS:41B6 */
    uint16_t level_resource_segment;    /* DS:41CE */
    uint16_t selected_game_mode;        /* DS:4526 */
    uint16_t collision_resolution_enabled; /* DS:457E */
    uint16_t vga_scratch_segment;       /* DS:5486 */
    uint16_t current_level;             /* DS:933E */
    uint16_t completion_count[30];      /* DS:452A */
    uint16_t game_mode;                 /* DS:9602 */
} SrRecoveredGlobals;

/*
 * Platform/resource operations reached by the recovered main control flow.
 * Original CS offsets remain in comments where the portable implementation
 * lives in another reconstruction module.
 */
typedef struct SrRecoveredOps {
    void *context;
    int (*detect_vga)(void *context); /* 5FFD */
    int (*detect_ega)(void *context); /* 5FE9 */
    void (*print_missing_video)(void *context); /* call through DOS output at 5C43 */
    void (*detect_sound_blaster)(void *context); /* 5A7C */
    void (*initialize_opl)(void *context); /* 58B1 */
    void (*install_timer_interrupts)(void *context); /* 3AB0 */
    void (*initialize_video)(void *context); /* 0000 */
    void (*load_config)(void *context); /* 571B */
    void (*load_music_track)(void *context, uint16_t track); /* 57A8 */
    void (*load_dashboard_tables_and_demo)(void *context); /* 54EC */
    void (*load_trekdat)(void *context); /* 00BB */
    void (*fatal_if_io_error)(void *context); /* 0069 */
    void (*initialize_road_renderer)(void *context); /* 2CB2 */
    int (*play_intro_sequence)(void *context); /* 4575 */
    void (*show_main_menu)(void *context, int first_pass_is_zero); /* 4E36 */
    void (*allocation_scope_push)(void *context); /* 3F04 */
    void (*load_graphics_archives)(void *context); /* 554B */
    int (*show_level_selection)(void *context); /* 5164 */
    void (*allocation_scope_pop)(void *context); /* 3F1F */
    uint16_t (*sample_pit_seed)(void *context); /* 019C */
    uint16_t (*load_road)(void *context, uint16_t index); /* 55F8 */
    void (*load_world)(void *context, uint16_t world); /* 536B */
    uint16_t (*allocation_stack_push_bytes)(void *context, uint32_t bytes); /* 3ED8 */
    void (*initialize_level_input)(void *context); /* 0707 */
    void (*drain_keyboard_and_latch_intro_abort)(void *context); /* 4137 */
    int (*run_level)(void *context, int final_unfinished_level); /* 2B21 */
    void (*save_config)(void *context); /* 5770 */
} SrRecoveredOps;

/* Control-flow reconstruction of skyroads.exe 1000:01B8. */
void sr_game_main_recovered(SrRecoveredGlobals *globals, const SrRecoveredOps *ops);

#endif
