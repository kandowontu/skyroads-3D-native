#include "menus.h"

#include <string.h>

static SrMainMenuAction main_menu_action(uint16_t selection) {
    if (selection == 0) return SR_MAIN_MENU_START;
    if (selection == 1) return SR_MAIN_MENU_SETTINGS;
    return SR_MAIN_MENU_HELP;
}

void sr_main_menu_init(SrMainMenuState *state) {
    if (state == 0) return;
    state->selection = 0;
}

SrMainMenuEvent sr_main_menu_key(SrMainMenuState *state, uint16_t key) {
    SrMainMenuEvent event;
    memset(&event, 0, sizeof(event));
    if (state == 0) return event;
    if (state->selection >= SR_MAIN_MENU_ITEM_COUNT) state->selection = 0;
    switch (key) {
        case SR_MENU_KEY_UP:
            if (state->selection != 0) --state->selection;
            break;
        case SR_MENU_KEY_DOWN:
            if (state->selection < SR_MAIN_MENU_ITEM_COUNT - 1) {
                ++state->selection;
            }
            break;
        case SR_MENU_KEY_ESCAPE:
            event.flush_keyboard = 1;
            event.action = main_menu_action(state->selection);
            break;
        case SR_MENU_KEY_ENTER:
            event.action = main_menu_action(state->selection);
            break;
        default:
            break;
    }
    return event;
}

void sr_settings_menu_init(
    SrSettingsMenuState *state,
    uint16_t selected_input_mode,
    uint16_t sound_disabled) {
    if (state == 0) return;
    state->selection = 0;
    state->selected_input_mode = selected_input_mode;
    state->sound_disabled = sound_disabled;
}

SrSettingsMenuEvent sr_settings_menu_key(
    SrSettingsMenuState *state,
    uint16_t key) {
    SrSettingsMenuEvent event;
    memset(&event, 0, sizeof(event));
    if (state == 0) return event;
    if (state->selection >= SR_SETTINGS_MENU_ITEM_COUNT) state->selection = 0;
    switch (key) {
        case SR_MENU_KEY_ENTER:
            if (state->selection <= 2) {
                state->selected_input_mode = state->selection;
            }
            else {
                state->sound_disabled = (uint16_t)(state->selection - 3u);
            }
            event.action = SR_SETTINGS_MENU_CONFIG_UPDATED;
            event.music_action = state->sound_disabled
                ? SR_SETTINGS_MUSIC_SILENCE
                : SR_SETTINGS_MUSIC_LOAD_TRACK_ONE;
            break;
        case SR_MENU_KEY_ESCAPE:
            event.action = SR_SETTINGS_MENU_EXIT;
            break;
        case SR_MENU_KEY_LEFT:
            if (state->selection != 0) --state->selection;
            break;
        case SR_MENU_KEY_RIGHT:
            if (state->selection < SR_SETTINGS_MENU_ITEM_COUNT - 1) {
                ++state->selection;
            }
            break;
        case SR_MENU_KEY_UP:
            if (state->selection == 3) state->selection = 0;
            else if (state->selection == 4) state->selection = 1;
            break;
        case SR_MENU_KEY_DOWN:
            if (state->selection == 0) state->selection = 3;
            else if (state->selection < 3) state->selection = 4;
            break;
        default:
            break;
    }
    return event;
}

unsigned sr_help_menu_page_count(uint16_t first_page_key) {
    return first_page_key == SR_MENU_KEY_ESCAPE ? 1u : 2u;
}

void sr_level_menu_init(SrLevelMenuState *state, uint16_t current_level) {
    if (state == 0) return;
    state->current_level = current_level;
}

SrLevelMenuAction sr_level_menu_key(SrLevelMenuState *state, uint16_t key) {
    if (state == 0) return SR_LEVEL_MENU_NONE;

    /* 527F clamps at the beginning of each input-loop iteration. */
    if (state->current_level >= SR_LEVEL_COUNT) {
        state->current_level = SR_LEVEL_COUNT - 1;
    }
    switch (key) {
        case SR_MENU_KEY_ENTER:
            return SR_LEVEL_MENU_START;
        case SR_MENU_KEY_ESCAPE:
            return SR_LEVEL_MENU_BACK;
        case SR_MENU_KEY_UP:
            if (state->current_level != 0) --state->current_level;
            break;
        case SR_MENU_KEY_LEFT:
            if (state->current_level < 15) state->current_level = 0;
            else state->current_level = (uint16_t)(state->current_level - 15u);
            break;
        case SR_MENU_KEY_RIGHT:
            state->current_level = (uint16_t)(state->current_level + 15u);
            break;
        case SR_MENU_KEY_DOWN:
            state->current_level = (uint16_t)(state->current_level + 1u);
            break;
        default:
            break;
    }
    return SR_LEVEL_MENU_NONE;
}

unsigned sr_level_completion_marker_count(uint16_t completion_count) {
    return completion_count < SR_LEVEL_COMPLETION_MARKER_LIMIT
        ? completion_count
        : SR_LEVEL_COMPLETION_MARKER_LIMIT;
}

int sr_level_menu_layout(unsigned level, SrLevelMenuLayout *layout) {
    unsigned slot;
    unsigned x;
    unsigned y;
    if (layout == 0 || level >= SR_LEVEL_COUNT) return 0;
    slot = level % 15u;
    x = level < 15u ? 62u : 222u;
    y = 12u + (slot % 3u) * 9u + (slot / 3u) * 39u;
    layout->selector_x = (uint16_t)x;
    layout->selector_y = (uint16_t)y;
    layout->completion_x = (uint16_t)(x + 50u);
    layout->completion_y = (uint16_t)(y + 2u);
    return 1;
}
