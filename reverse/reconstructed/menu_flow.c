#include "menu_flow.h"

#include "palette.h"

#include <string.h>

enum { SR_MENU_FLOW_FADE_TICKS = 0x24 };

static void initialize(
    SrMenuFlowState *state,
    SrMenuFlowScreen screen,
    SrMenuFlowPhase phase) {
    if (state == 0) return;
    memset(state, 0, sizeof(*state));
    state->screen = screen;
    state->phase = phase;
}

static void begin_fade_out(
    SrMenuFlowState *state,
    SrMenuFlowRoute route) {
    if (state == 0 || state->phase != SR_MENU_FLOW_STEADY) return;
    state->phase = SR_MENU_FLOW_FADE_OUT;
    state->route = route;
    state->phase_ticks = 0;
}

void sr_menu_flow_after_intro(SrMenuFlowState *state) {
    initialize(state, SR_MENU_FLOW_MAIN, SR_MENU_FLOW_STEADY);
}

void sr_menu_flow_enter_main(SrMenuFlowState *state) {
    initialize(state, SR_MENU_FLOW_MAIN, SR_MENU_FLOW_FADE_IN);
}

void sr_menu_flow_enter_levels(SrMenuFlowState *state) {
    initialize(state, SR_MENU_FLOW_LEVELS, SR_MENU_FLOW_FADE_IN);
}

int sr_menu_flow_accepts_input(const SrMenuFlowState *state) {
    return state != 0 && state->phase == SR_MENU_FLOW_STEADY;
}

void sr_menu_flow_main_action(
    SrMenuFlowState *state,
    SrMainMenuAction action) {
    if (state == 0 || state->screen != SR_MENU_FLOW_MAIN) return;
    if (action == SR_MAIN_MENU_START) {
        begin_fade_out(state, SR_MENU_FLOW_ROUTE_LEVELS);
    }
    else if (action == SR_MAIN_MENU_SETTINGS) {
        begin_fade_out(state, SR_MENU_FLOW_ROUTE_SETTINGS);
    }
    else if (action == SR_MAIN_MENU_HELP) {
        begin_fade_out(state, SR_MENU_FLOW_ROUTE_HELP_ONE);
    }
}

void sr_menu_flow_settings_exit(SrMenuFlowState *state) {
    if (state == 0 || state->screen != SR_MENU_FLOW_SETTINGS) return;
    begin_fade_out(state, SR_MENU_FLOW_ROUTE_MAIN);
}

void sr_menu_flow_help_key(SrMenuFlowState *state, uint16_t key) {
    if (state == 0) return;
    if (state->screen == SR_MENU_FLOW_HELP_ONE) {
        begin_fade_out(state, key == SR_MENU_KEY_ESCAPE
            ? SR_MENU_FLOW_ROUTE_MAIN
            : SR_MENU_FLOW_ROUTE_HELP_TWO);
    }
    else if (state->screen == SR_MENU_FLOW_HELP_TWO) {
        begin_fade_out(state, SR_MENU_FLOW_ROUTE_MAIN);
    }
}

void sr_menu_flow_level_action(
    SrMenuFlowState *state,
    SrLevelMenuAction action) {
    if (state == 0 || state->screen != SR_MENU_FLOW_LEVELS) return;
    if (action == SR_LEVEL_MENU_START) {
        begin_fade_out(state, SR_MENU_FLOW_ROUTE_START_LEVEL);
    }
    else if (action == SR_LEVEL_MENU_BACK) {
        begin_fade_out(state, SR_MENU_FLOW_ROUTE_MAIN);
    }
}

static void complete_route(SrMenuFlowState *state, SrMenuFlowStep *step) {
    switch (state->route) {
        case SR_MENU_FLOW_ROUTE_SETTINGS:
            state->screen = SR_MENU_FLOW_SETTINGS;
            break;
        case SR_MENU_FLOW_ROUTE_HELP_ONE:
            state->screen = SR_MENU_FLOW_HELP_ONE;
            break;
        case SR_MENU_FLOW_ROUTE_HELP_TWO:
            state->screen = SR_MENU_FLOW_HELP_TWO;
            break;
        case SR_MENU_FLOW_ROUTE_LEVELS:
            state->screen = SR_MENU_FLOW_LEVELS;
            break;
        case SR_MENU_FLOW_ROUTE_MAIN:
            state->screen = SR_MENU_FLOW_MAIN;
            break;
        case SR_MENU_FLOW_ROUTE_START_LEVEL:
            state->phase = SR_MENU_FLOW_STEADY;
            state->route = SR_MENU_FLOW_ROUTE_NONE;
            state->phase_ticks = 0;
            step->events |= SR_MENU_FLOW_EVENT_START_LEVEL;
            return;
        default:
            state->phase = SR_MENU_FLOW_STEADY;
            state->route = SR_MENU_FLOW_ROUTE_NONE;
            state->phase_ticks = 0;
            return;
    }
    state->phase = SR_MENU_FLOW_FADE_IN;
    state->route = SR_MENU_FLOW_ROUTE_NONE;
    state->phase_ticks = 0;
    step->events |= SR_MENU_FLOW_EVENT_DRAW_SCREEN;
}

SrMenuFlowStep sr_menu_flow_tick(SrMenuFlowState *state) {
    SrMenuFlowStep step;
    memset(&step, 0, sizeof(step));
    if (state == 0) return step;
    step.screen = state->screen;
    step.phase = state->phase;
    step.palette_percent = state->phase == SR_MENU_FLOW_FADE_OUT ? 100u : 0u;
    if (state->phase == SR_MENU_FLOW_STEADY) {
        step.palette_percent = 100u;
        return step;
    }

    ++state->phase_ticks;
    if (state->phase == SR_MENU_FLOW_FADE_IN) {
        step.palette_percent = sr_palette_transition_percent(
            state->phase_ticks, SR_MENU_FLOW_FADE_TICKS);
        if (state->phase_ticks >= SR_MENU_FLOW_FADE_TICKS) {
            state->phase = SR_MENU_FLOW_STEADY;
            state->phase_ticks = 0;
        }
    }
    else {
        step.palette_percent = (uint16_t)(100u - sr_palette_transition_percent(
            state->phase_ticks, SR_MENU_FLOW_FADE_TICKS));
        if (state->phase_ticks >= SR_MENU_FLOW_FADE_TICKS) {
            complete_route(state, &step);
            step.palette_percent = 0;
        }
    }
    step.screen = state->screen;
    step.phase = state->phase;
    return step;
}
