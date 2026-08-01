#ifndef SKYROADS_RECOVERED_MENUS_H
#define SKYROADS_RECOVERED_MENUS_H

#include <stdint.h>

enum {
    SR_MENU_KEY_ENTER = 0x000d,
    SR_MENU_KEY_ESCAPE = 0x001b,
    SR_MENU_KEY_UP = 0x4800,
    SR_MENU_KEY_LEFT = 0x4b00,
    SR_MENU_KEY_RIGHT = 0x4d00,
    SR_MENU_KEY_DOWN = 0x5000,
    SR_MAIN_MENU_ITEM_COUNT = 3,
    SR_SETTINGS_MENU_ITEM_COUNT = 5,
    SR_LEVEL_COUNT = 30,
    SR_LEVEL_COMPLETION_MARKER_LIMIT = 7
};

typedef enum SrMainMenuAction {
    SR_MAIN_MENU_NONE = 0,
    SR_MAIN_MENU_START,
    SR_MAIN_MENU_SETTINGS,
    SR_MAIN_MENU_HELP
} SrMainMenuAction;

typedef struct SrMainMenuState {
    uint16_t selection;
} SrMainMenuState;

typedef struct SrMainMenuEvent {
    SrMainMenuAction action;
    uint8_t flush_keyboard;
} SrMainMenuEvent;

typedef enum SrSettingsMenuAction {
    SR_SETTINGS_MENU_NONE = 0,
    SR_SETTINGS_MENU_CONFIG_UPDATED,
    SR_SETTINGS_MENU_EXIT
} SrSettingsMenuAction;

typedef enum SrSettingsMusicAction {
    SR_SETTINGS_MUSIC_UNCHANGED = 0,
    SR_SETTINGS_MUSIC_LOAD_TRACK_ONE,
    SR_SETTINGS_MUSIC_SILENCE
} SrSettingsMusicAction;

typedef struct SrSettingsMenuState {
    uint16_t selection;
    uint16_t selected_input_mode;
    uint16_t sound_disabled;
} SrSettingsMenuState;

typedef struct SrSettingsMenuEvent {
    SrSettingsMenuAction action;
    SrSettingsMusicAction music_action;
} SrSettingsMenuEvent;

typedef enum SrLevelMenuAction {
    SR_LEVEL_MENU_NONE = 0,
    SR_LEVEL_MENU_START,
    SR_LEVEL_MENU_BACK
} SrLevelMenuAction;

typedef struct SrLevelMenuState {
    uint16_t current_level;
} SrLevelMenuState;

typedef struct SrLevelMenuLayout {
    uint16_t selector_x;
    uint16_t selector_y;
    uint16_t completion_x;
    uint16_t completion_y;
} SrLevelMenuLayout;

/* Keyboard/state dispatch from skyroads.exe 1000:4E36. */
void sr_main_menu_init(SrMainMenuState *state);
SrMainMenuEvent sr_main_menu_key(SrMainMenuState *state, uint16_t key);

/* Keyboard/state dispatch from skyroads.exe 1000:4C04. */
void sr_settings_menu_init(
    SrSettingsMenuState *state,
    uint16_t selected_input_mode,
    uint16_t sound_disabled);
SrSettingsMenuEvent sr_settings_menu_key(
    SrSettingsMenuState *state,
    uint16_t key);

/* 4E12 displays page two unless Escape dismissed page one. */
unsigned sr_help_menu_page_count(uint16_t first_page_key);

/* Keyboard/state and geometry from skyroads.exe 1000:5064/5164. */
void sr_level_menu_init(SrLevelMenuState *state, uint16_t current_level);
SrLevelMenuAction sr_level_menu_key(SrLevelMenuState *state, uint16_t key);
unsigned sr_level_completion_marker_count(uint16_t completion_count);
int sr_level_menu_layout(unsigned level, SrLevelMenuLayout *layout);

#endif
