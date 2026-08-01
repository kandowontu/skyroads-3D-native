#include "menus.h"

int main(void) {
    SrMainMenuState main_menu;
    SrMainMenuEvent main_event;
    SrSettingsMenuState settings;
    SrSettingsMenuEvent settings_event;
    SrLevelMenuState levels;
    SrLevelMenuLayout layout;

    sr_main_menu_init(&main_menu);
    main_event = sr_main_menu_key(&main_menu, SR_MENU_KEY_UP);
    if (main_menu.selection != 0 || main_event.action != SR_MAIN_MENU_NONE) return 1;
    sr_main_menu_key(&main_menu, SR_MENU_KEY_DOWN);
    sr_main_menu_key(&main_menu, SR_MENU_KEY_DOWN);
    sr_main_menu_key(&main_menu, SR_MENU_KEY_DOWN);
    if (main_menu.selection != 2) return 1;
    main_event = sr_main_menu_key(&main_menu, SR_MENU_KEY_ESCAPE);
    if (main_event.action != SR_MAIN_MENU_HELP || !main_event.flush_keyboard) return 1;
    sr_main_menu_key(&main_menu, SR_MENU_KEY_UP);
    main_event = sr_main_menu_key(&main_menu, SR_MENU_KEY_ENTER);
    if (main_event.action != SR_MAIN_MENU_SETTINGS || main_event.flush_keyboard) return 1;

    sr_settings_menu_init(&settings, 2, 0);
    if (settings.selection != 0 || settings.selected_input_mode != 2) return 1;
    sr_settings_menu_key(&settings, SR_MENU_KEY_DOWN);
    if (settings.selection != 3) return 1;
    sr_settings_menu_key(&settings, SR_MENU_KEY_UP);
    sr_settings_menu_key(&settings, SR_MENU_KEY_RIGHT);
    sr_settings_menu_key(&settings, SR_MENU_KEY_DOWN);
    if (settings.selection != 4) return 1;
    settings_event = sr_settings_menu_key(&settings, SR_MENU_KEY_ENTER);
    if (settings.sound_disabled != 1 ||
        settings_event.action != SR_SETTINGS_MENU_CONFIG_UPDATED ||
        settings_event.music_action != SR_SETTINGS_MUSIC_SILENCE) return 1;
    sr_settings_menu_key(&settings, SR_MENU_KEY_LEFT);
    settings_event = sr_settings_menu_key(&settings, SR_MENU_KEY_ENTER);
    if (settings.sound_disabled != 0 ||
        settings_event.music_action != SR_SETTINGS_MUSIC_LOAD_TRACK_ONE) return 1;
    sr_settings_menu_key(&settings, SR_MENU_KEY_UP);
    sr_settings_menu_key(&settings, SR_MENU_KEY_RIGHT);
    settings_event = sr_settings_menu_key(&settings, SR_MENU_KEY_ENTER);
    if (settings.selected_input_mode != 1 ||
        settings_event.music_action != SR_SETTINGS_MUSIC_LOAD_TRACK_ONE) return 1;
    settings_event = sr_settings_menu_key(&settings, SR_MENU_KEY_ESCAPE);
    if (settings_event.action != SR_SETTINGS_MENU_EXIT) return 1;
    if (sr_help_menu_page_count(SR_MENU_KEY_ESCAPE) != 1 ||
        sr_help_menu_page_count(SR_MENU_KEY_ENTER) != 2) return 1;

    sr_level_menu_init(&levels, 29);
    sr_level_menu_key(&levels, SR_MENU_KEY_RIGHT);
    if (levels.current_level != 44) return 1;
    sr_level_menu_key(&levels, SR_MENU_KEY_DOWN);
    if (levels.current_level != 30) return 1;
    sr_level_menu_key(&levels, SR_MENU_KEY_UP);
    if (levels.current_level != 28) return 1;
    sr_level_menu_key(&levels, SR_MENU_KEY_LEFT);
    if (levels.current_level != 13) return 1;
    sr_level_menu_key(&levels, SR_MENU_KEY_LEFT);
    if (levels.current_level != 0) return 1;
    if (sr_level_menu_key(&levels, SR_MENU_KEY_ENTER) != SR_LEVEL_MENU_START ||
        sr_level_menu_key(&levels, SR_MENU_KEY_ESCAPE) != SR_LEVEL_MENU_BACK) return 1;

    if (sr_level_completion_marker_count(0) != 0 ||
        sr_level_completion_marker_count(5) != 5 ||
        sr_level_completion_marker_count(8) != 7) return 1;
    if (!sr_level_menu_layout(0, &layout) ||
        layout.selector_x != 62 || layout.selector_y != 12 ||
        layout.completion_x != 112 || layout.completion_y != 14) return 1;
    if (!sr_level_menu_layout(14, &layout) ||
        layout.selector_x != 62 || layout.selector_y != 186) return 1;
    if (!sr_level_menu_layout(15, &layout) ||
        layout.selector_x != 222 || layout.selector_y != 12 ||
        layout.completion_x != 272 || layout.completion_y != 14) return 1;
    if (sr_level_menu_layout(30, &layout)) return 1;
    return 0;
}
