#include "input.h"

static int pressed(const SrLevelInput *input, unsigned index) {
    return (input->key_flags[index] & 0x80u) != 0;
}

static int any3(int a, int b, int c) {
    return a || b || c;
}

int sr_sample_level_input(SrLevelInput *input, const SrInputDevices *devices) {
    if (input->mode == SR_INPUT_KEYBOARD) {
        input->steering = (int16_t)(
            any3(pressed(input, 3), pressed(input, 5), pressed(input, 7)) -
            any3(pressed(input, 2), pressed(input, 4), pressed(input, 6)));
        input->throttle = (int16_t)(
            any3(pressed(input, 0), pressed(input, 4), pressed(input, 5)) -
            any3(pressed(input, 1), pressed(input, 6), pressed(input, 7)));
        input->jump = (uint16_t)pressed(input, 9);
        return 1;
    }

    if (input->mode == SR_INPUT_JOYSTICK) {
        uint16_t x = devices->joystick_axis(devices->context, 1);
        uint16_t y = devices->joystick_axis(devices->context, 2);
        uint16_t upper_x = (uint16_t)(input->joystick_center_x * 3u);
        uint16_t upper_y = (uint16_t)(input->joystick_center_y * 3u);
        upper_x = (uint16_t)(upper_x / 2u);
        upper_y = (uint16_t)(upper_y / 2u);
        input->steering = (int16_t)(
            (x > upper_x) -
            (x < input->joystick_center_x / 2u));
        input->throttle = (int16_t)(
            (y < input->joystick_center_y / 2u) -
            (y > upper_y));
        input->jump = devices->joystick_button(devices->context);
        return 1;
    }

    if (input->mode == SR_INPUT_MOUSE) {
        uint16_t x0 = devices->mouse_value(devices->context, 0);
        uint16_t x1 = devices->mouse_value(devices->context, 0);
        uint16_t y0;
        uint16_t y1;
        input->steering = (int16_t)((x1 > 0xaa) - (x0 < 0x96));
        y0 = devices->mouse_value(devices->context, 1);
        y1 = devices->mouse_value(devices->context, 1);
        input->throttle = (int16_t)((y1 < 0x0f) - (y0 > 0xb9));
        input->jump = devices->mouse_value(devices->context, 2);
        devices->mouse_set_position(
            devices->context, 0x00a0, devices->mouse_value(devices->context, 1));
        return 1;
    }

    if (input->mode == SR_INPUT_DEMO) {
        size_t index = (size_t)(input->road_distance / UINT32_C(0x666));
        uint8_t encoded;
        if (index >= input->demo_record_size) {
            return 0;
        }
        encoded = input->demo_record[index];
        input->throttle = (int16_t)((encoded & 3u) - 1u);
        input->steering = (int16_t)(((encoded >> 2) & 3u) - 1u);
        input->jump = (encoded >> 4) & 1u;
        return 1;
    }

    return 0;
}
