#ifndef SKYROADS_RECOVERED_MENU_FLOW_H
#define SKYROADS_RECOVERED_MENU_FLOW_H

#include <stdint.h>

#include "menus.h"

typedef enum SrMenuFlowScreen {
    SR_MENU_FLOW_MAIN = 0,
    SR_MENU_FLOW_SETTINGS,
    SR_MENU_FLOW_HELP_ONE,
    SR_MENU_FLOW_HELP_TWO,
    SR_MENU_FLOW_LEVELS
} SrMenuFlowScreen;

typedef enum SrMenuFlowPhase {
    SR_MENU_FLOW_STEADY = 0,
    SR_MENU_FLOW_FADE_IN,
    SR_MENU_FLOW_FADE_OUT
} SrMenuFlowPhase;

typedef enum SrMenuFlowRoute {
    SR_MENU_FLOW_ROUTE_NONE = 0,
    SR_MENU_FLOW_ROUTE_SETTINGS,
    SR_MENU_FLOW_ROUTE_HELP_ONE,
    SR_MENU_FLOW_ROUTE_HELP_TWO,
    SR_MENU_FLOW_ROUTE_LEVELS,
    SR_MENU_FLOW_ROUTE_MAIN,
    SR_MENU_FLOW_ROUTE_START_LEVEL
} SrMenuFlowRoute;

enum SrMenuFlowEventFlag {
    SR_MENU_FLOW_EVENT_NONE = 0,
    SR_MENU_FLOW_EVENT_DRAW_SCREEN = 1 << 0,
    SR_MENU_FLOW_EVENT_START_LEVEL = 1 << 1
};

typedef struct SrMenuFlowState {
    SrMenuFlowScreen screen;
    SrMenuFlowPhase phase;
    SrMenuFlowRoute route;
    uint16_t phase_ticks;
} SrMenuFlowState;

typedef struct SrMenuFlowStep {
    uint16_t events;
    SrMenuFlowScreen screen;
    SrMenuFlowPhase phase;
    uint16_t palette_percent;
} SrMenuFlowStep;

/* show_main_menu(0): the first menu entered directly from the intro. */
void sr_menu_flow_after_intro(SrMenuFlowState *state);

/* show_main_menu(1) and show_level_selection: draw black, then fade in. */
void sr_menu_flow_enter_main(SrMenuFlowState *state);
void sr_menu_flow_enter_levels(SrMenuFlowState *state);

int sr_menu_flow_accepts_input(const SrMenuFlowState *state);

/* Blocking-call routes recovered from 1000:4C04-1000:536A. */
void sr_menu_flow_main_action(
    SrMenuFlowState *state,
    SrMainMenuAction action);
void sr_menu_flow_settings_exit(SrMenuFlowState *state);
void sr_menu_flow_help_key(SrMenuFlowState *state, uint16_t key);
void sr_menu_flow_level_action(
    SrMenuFlowState *state,
    SrLevelMenuAction action);

/* Advances one original 36 Hz palette-transition tick. */
SrMenuFlowStep sr_menu_flow_tick(SrMenuFlowState *state);

#endif
