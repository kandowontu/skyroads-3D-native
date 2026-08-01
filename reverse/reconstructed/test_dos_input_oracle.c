#include "input.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct OracleDevices {
    uint16_t joystick_x;
    uint16_t joystick_y;
    uint16_t joystick_button_value;
    uint16_t mouse_x[2];
    uint16_t mouse_y[3];
    uint16_t mouse_button_value;
    unsigned mouse_x_calls;
    unsigned mouse_y_calls;
    uint16_t set_x;
    uint16_t set_y;
} OracleDevices;

static uint16_t read_u16(const uint8_t *bytes) {
    return (uint16_t)(bytes[0] | (uint16_t)bytes[1] << 8u);
}

static uint8_t *read_file(const char *path, size_t expected_size) {
    FILE *stream = fopen(path, "rb");
    uint8_t *bytes;
    long size;
    if (stream == NULL) return NULL;
    if (fseek(stream, 0, SEEK_END) != 0 || (size = ftell(stream)) < 0 ||
        (size_t)size != expected_size || fseek(stream, 0, SEEK_SET) != 0) {
        fclose(stream);
        return NULL;
    }
    bytes = (uint8_t *)malloc(expected_size);
    if (bytes == NULL || fread(bytes, 1, expected_size, stream) != expected_size) {
        free(bytes);
        fclose(stream);
        return NULL;
    }
    fclose(stream);
    return bytes;
}

static uint16_t joystick_axis(void *context, unsigned axis) {
    OracleDevices *devices = (OracleDevices *)context;
    return axis == 1 ? devices->joystick_x : devices->joystick_y;
}

static uint16_t joystick_button(void *context) {
    return ((OracleDevices *)context)->joystick_button_value;
}

static uint16_t mouse_value(void *context, unsigned selector) {
    OracleDevices *devices = (OracleDevices *)context;
    if (selector == 0) {
        unsigned index = devices->mouse_x_calls++;
        if (index > 1) index = 1;
        return devices->mouse_x[index];
    }
    if (selector == 1) {
        unsigned index = devices->mouse_y_calls++;
        if (index > 2) index = 2;
        return devices->mouse_y[index];
    }
    return devices->mouse_button_value;
}

static void mouse_set_position(void *context, uint16_t x, uint16_t y) {
    OracleDevices *devices = (OracleDevices *)context;
    devices->set_x = x;
    devices->set_y = y;
}

static int output_matches(const SrLevelInput *input, const uint8_t *record) {
    return input->steering == (int16_t)read_u16(record) &&
        input->throttle == (int16_t)read_u16(record + 2) &&
        input->jump == read_u16(record + 4);
}

int main(int argc, char **argv) {
    enum {
        KEY_RECORDS = 2048,
        KEY_RECORD_BYTES = 6,
        AXIS_VALUES = 18,
        JOY_RECORDS = 2 * AXIS_VALUES * AXIS_VALUES,
        JOY_RECORD_BYTES = 14,
        MOUSE_RECORDS = 4 * 4 * 4 * 4 * 2,
        MOUSE_RECORD_BYTES = 22
    };
    uint8_t *key_bytes;
    uint8_t *joy_bytes;
    uint8_t *mouse_bytes;
    OracleDevices fake;
    SrInputDevices devices;
    SrLevelInput input;
    unsigned record;
    if (argc != 4) {
        fputs("usage: test_dos_input_oracle ORAKEY.BIN ORAJOY.BIN ORAMOUSE.BIN\n", stderr);
        return 1;
    }
    key_bytes = read_file(argv[1], KEY_RECORDS * KEY_RECORD_BYTES);
    joy_bytes = read_file(argv[2], JOY_RECORDS * JOY_RECORD_BYTES);
    mouse_bytes = read_file(argv[3], MOUSE_RECORDS * MOUSE_RECORD_BYTES);
    if (key_bytes == NULL || joy_bytes == NULL || mouse_bytes == NULL) {
        fputs("Could not load DOS input oracle files\n", stderr);
        free(key_bytes);
        free(joy_bytes);
        free(mouse_bytes);
        return 1;
    }
    memset(&fake, 0, sizeof(fake));
    devices.context = &fake;
    devices.joystick_axis = joystick_axis;
    devices.joystick_button = joystick_button;
    devices.mouse_value = mouse_value;
    devices.mouse_set_position = mouse_set_position;

    for (record = 0; record < KEY_RECORDS; ++record) {
        unsigned key;
        const uint8_t *expected = key_bytes + record * KEY_RECORD_BYTES;
        memset(&input, 0, sizeof(input));
        input.mode = SR_INPUT_KEYBOARD;
        for (key = 0; key < 11; ++key) {
            input.key_flags[key] = (record & (1u << key)) != 0 ? 0x80u : 0;
        }
        if (!sr_sample_level_input(&input, &devices) ||
            !output_matches(&input, expected)) {
            fprintf(stderr,
                "DOS keyboard oracle differs at mask %04x: expected %d/%d/%u, got %d/%d/%u\n",
                record, (int16_t)read_u16(expected),
                (int16_t)read_u16(expected + 2), read_u16(expected + 4),
                input.steering, input.throttle, input.jump);
            return 1;
        }
    }

    for (record = 0; record < JOY_RECORDS; ++record) {
        const uint8_t *source = joy_bytes + record * JOY_RECORD_BYTES;
        const uint16_t kind = read_u16(source);
        const uint16_t center = read_u16(source + 2);
        const uint16_t value = read_u16(source + 4);
        memset(&input, 0, sizeof(input));
        input.mode = SR_INPUT_JOYSTICK;
        input.joystick_center_x = kind == 0 ? center : 100;
        input.joystick_center_y = kind == 0 ? 100 : center;
        fake.joystick_x = kind == 0 ? value : 100;
        fake.joystick_y = kind == 0 ? 100 : value;
        fake.joystick_button_value = read_u16(source + 6);
        if (!sr_sample_level_input(&input, &devices) ||
            !output_matches(&input, source + 8)) {
            fprintf(stderr,
                "DOS joystick oracle differs at record %u kind=%u center=%u value=%u: "
                "expected %d/%d/%u, got %d/%d/%u\n",
                record, kind, center, value, (int16_t)read_u16(source + 8),
                (int16_t)read_u16(source + 10), read_u16(source + 12),
                input.steering, input.throttle, input.jump);
            return 1;
        }
    }

    for (record = 0; record < MOUSE_RECORDS; ++record) {
        const uint8_t *source = mouse_bytes + record * MOUSE_RECORD_BYTES;
        memset(&input, 0, sizeof(input));
        input.mode = SR_INPUT_MOUSE;
        fake.mouse_x[0] = read_u16(source);
        fake.mouse_x[1] = read_u16(source + 2);
        fake.mouse_y[0] = read_u16(source + 4);
        fake.mouse_y[1] = read_u16(source + 6);
        fake.mouse_y[2] = read_u16(source + 8);
        fake.mouse_button_value = read_u16(source + 10);
        fake.mouse_x_calls = 0;
        fake.mouse_y_calls = 0;
        fake.set_x = 0xffff;
        fake.set_y = 0xffff;
        if (!sr_sample_level_input(&input, &devices) ||
            !output_matches(&input, source + 12) ||
            fake.set_x != read_u16(source + 18) ||
            fake.set_y != read_u16(source + 20)) {
            fprintf(stderr,
                "DOS mouse oracle differs at record %u: expected %d/%d/%u set=%u/%u, "
                "got %d/%d/%u set=%u/%u\n",
                record, (int16_t)read_u16(source + 12),
                (int16_t)read_u16(source + 14), read_u16(source + 16),
                read_u16(source + 18), read_u16(source + 20),
                input.steering, input.throttle, input.jump,
                fake.set_x, fake.set_y);
            return 1;
        }
    }

    printf("DOS input oracle matched %u keyboard, %u joystick, and %u mouse vectors\n",
        KEY_RECORDS, JOY_RECORDS, MOUSE_RECORDS);
    free(key_bytes);
    free(joy_bytes);
    free(mouse_bytes);
    return 0;
}
