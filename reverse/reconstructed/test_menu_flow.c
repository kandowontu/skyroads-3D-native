#include "menu_flow.h"

static int advance(SrMenuFlowState *state, unsigned count, SrMenuFlowStep *last) {
    unsigned tick;
    for (tick = 0; tick < count; ++tick) *last = sr_menu_flow_tick(state);
    return 1;
}

int main(void) {
    SrMenuFlowState state;
    SrMenuFlowStep step;

    sr_menu_flow_after_intro(&state);
    if (state.screen != SR_MENU_FLOW_MAIN || !sr_menu_flow_accepts_input(&state)) return 1;

    sr_menu_flow_main_action(&state, SR_MAIN_MENU_SETTINGS);
    advance(&state, 35, &step);
    if (step.palette_percent == 0 || sr_menu_flow_accepts_input(&state)) return 1;
    advance(&state, 1, &step);
    if (state.screen != SR_MENU_FLOW_SETTINGS ||
        state.phase != SR_MENU_FLOW_FADE_IN ||
        step.events != SR_MENU_FLOW_EVENT_DRAW_SCREEN ||
        step.palette_percent != 0) return 1;
    advance(&state, 36, &step);
    if (!sr_menu_flow_accepts_input(&state) || step.palette_percent != 100) return 1;

    sr_menu_flow_settings_exit(&state);
    advance(&state, 36, &step);
    if (state.screen != SR_MENU_FLOW_MAIN ||
        step.events != SR_MENU_FLOW_EVENT_DRAW_SCREEN) return 1;
    advance(&state, 36, &step);

    sr_menu_flow_main_action(&state, SR_MAIN_MENU_HELP);
    advance(&state, 72, &step);
    if (state.screen != SR_MENU_FLOW_HELP_ONE || !sr_menu_flow_accepts_input(&state)) return 1;
    sr_menu_flow_help_key(&state, SR_MENU_KEY_ENTER);
    advance(&state, 36, &step);
    if (state.screen != SR_MENU_FLOW_HELP_TWO ||
        step.events != SR_MENU_FLOW_EVENT_DRAW_SCREEN) return 1;
    advance(&state, 36, &step);
    sr_menu_flow_help_key(&state, SR_MENU_KEY_ESCAPE);
    advance(&state, 72, &step);
    if (state.screen != SR_MENU_FLOW_MAIN || !sr_menu_flow_accepts_input(&state)) return 1;

    sr_menu_flow_main_action(&state, SR_MAIN_MENU_START);
    advance(&state, 72, &step);
    if (state.screen != SR_MENU_FLOW_LEVELS || !sr_menu_flow_accepts_input(&state)) return 1;
    sr_menu_flow_level_action(&state, SR_LEVEL_MENU_START);
    advance(&state, 35, &step);
    if ((step.events & SR_MENU_FLOW_EVENT_START_LEVEL) != 0) return 1;
    advance(&state, 1, &step);
    if (step.events != SR_MENU_FLOW_EVENT_START_LEVEL || step.palette_percent != 0) return 1;

    sr_menu_flow_enter_levels(&state);
    advance(&state, 36, &step);
    if (state.screen != SR_MENU_FLOW_LEVELS || !sr_menu_flow_accepts_input(&state)) return 1;
    sr_menu_flow_level_action(&state, SR_LEVEL_MENU_BACK);
    advance(&state, 72, &step);
    if (state.screen != SR_MENU_FLOW_MAIN || !sr_menu_flow_accepts_input(&state)) return 1;
    return 0;
}
