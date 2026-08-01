#ifndef SKYROADS_RECOVERED_INPUT_H
#define SKYROADS_RECOVERED_INPUT_H

#include <stddef.h>
#include <stdint.h>

typedef enum SrInputMode {
    SR_INPUT_KEYBOARD = 0,
    SR_INPUT_JOYSTICK = 1,
    SR_INPUT_MOUSE = 2,
    SR_INPUT_DEMO = 3
} SrInputMode;

typedef struct SrLevelInput {
    SrInputMode mode;              /* DS:9602 */
    int16_t steering;              /* DS:9600: -1, 0, +1 */
    int16_t throttle;              /* DS:933C: -1, 0, +1 */
    uint16_t jump;                 /* DS:5488 */
    uint8_t key_flags[11];         /* DS:0BA2..0BAC; bit 7 means pressed */
    uint16_t joystick_center_x;    /* DS:5180 */
    uint16_t joystick_center_y;    /* DS:548A */
    uint32_t road_distance;        /* DS:9628 low, DS:962A high */
    const uint8_t *demo_record;    /* DS:962E */
    size_t demo_record_size;
} SrLevelInput;

typedef struct SrInputDevices {
    void *context;
    uint16_t (*joystick_axis)(void *context, unsigned axis);
    uint16_t (*joystick_button)(void *context);
    uint16_t (*mouse_value)(void *context, unsigned selector);
    void (*mouse_set_position)(void *context, uint16_t x, uint16_t y);
} SrInputDevices;

/* Exact semantic reconstruction of skyroads.exe 1000:074C. */
int sr_sample_level_input(SrLevelInput *input, const SrInputDevices *devices);

#endif
