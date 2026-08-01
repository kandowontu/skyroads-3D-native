#include "startup.h"

void sr_game_main_recovered(SrRecoveredGlobals *g, const SrRecoveredOps *ops) {
    uint16_t last_random_track = 0xffff;
    int first_pass = 0;

    if (!ops->detect_vga(ops->context)) {
        g->video_is_vga = 0;
        if (!ops->detect_ega(ops->context)) {
            ops->print_missing_video(ops->context);
            return;
        }
    }

    ops->detect_sound_blaster(ops->context);
    ops->initialize_opl(ops->context);
    ops->install_timer_interrupts(ops->context);
    g->collision_resolution_enabled = 1;
    ops->initialize_video(ops->context);
    ops->load_config(ops->context);
    ops->load_music_track(ops->context, 0);
    ops->load_dashboard_tables_and_demo(ops->context);
    ops->load_trekdat(ops->context);
    ops->fatal_if_io_error(ops->context);
    ops->initialize_road_renderer(ops->context);

menu_entry_0219:
    first_pass = 1;
    if (!ops->play_intro_sequence(ops->context)) {
        g->game_mode = 3;
        goto resources_0251;
    }

apply_selected_mode_0232:
    g->game_mode = g->selected_game_mode;
    ops->show_main_menu(ops->context, first_pass == 0);

resources_0251:
    first_pass = 0;
    ops->allocation_scope_push(ops->context);
    ops->load_graphics_archives(ops->context);

    for (;;) {
        uint16_t completed;
        uint16_t index;
        int road_result;

        ops->allocation_scope_push(ops->context);
        if (g->game_mode == 3) {
            g->level_resource_segment = ops->load_road(ops->context, 0);
            ops->load_world(ops->context, 0);
        }
        else {
            uint16_t track;
            if (ops->show_level_selection(ops->context)) {
                ops->allocation_scope_pop(ops->context);
                ops->allocation_scope_pop(ops->context);
                goto apply_selected_mode_0232;
            }
            g->game_mode = g->selected_game_mode;
            track = (uint16_t)(ops->sample_pit_seed(ops->context) % 12);
            if (track == last_random_track) {
                track = (uint16_t)((track + 1) % 12);
            }
            last_random_track = track;
            ops->load_music_track(ops->context, (uint16_t)(track + 2));
            g->level_resource_segment =
                ops->load_road(ops->context, (uint16_t)(g->current_level + 1));
            ops->load_world(ops->context, (uint16_t)(g->current_level / 3));
        }

        if (g->video_is_vga) {
            g->vga_scratch_segment =
                ops->allocation_stack_push_bytes(ops->context, 0xac80u);
        }
        ops->fatal_if_io_error(ops->context);
        ops->initialize_level_input(ops->context);

        completed = 0;
        for (index = 0; index < 30; ++index) {
            if (g->completion_count[index] != 0) {
                ++completed;
            }
        }

        do {
            int final_unfinished_level;
            ops->drain_keyboard_and_latch_intro_abort(ops->context);
            final_unfinished_level =
                g->completion_count[g->current_level] == 0 && completed == 29;
            road_result = ops->run_level(ops->context, final_unfinished_level);

            if (g->game_mode == 3) {
                ops->allocation_scope_pop(ops->context);
                ops->allocation_scope_pop(ops->context);
                if (road_result == 7) {
                    goto apply_selected_mode_0232;
                }
                goto menu_entry_0219;
            }

            if (road_result == 0) {
                ++g->completion_count[g->current_level];
                ++g->current_level;
                ops->save_config(ops->context);
            }
        } while (road_result != 7 && road_result != 0);

        ops->allocation_scope_pop(ops->context);
    }
}
