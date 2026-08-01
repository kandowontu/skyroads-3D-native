#include "input.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct FakeDevices {
    uint16_t joystick[3];
    uint16_t mouse[3];
    uint16_t set_x;
    uint16_t set_y;
} FakeDevices;

static uint16_t joystick_axis(void *context, unsigned axis) {
    return ((FakeDevices *)context)->joystick[axis];
}

static uint16_t joystick_button(void *context) {
    (void)context;
    return 1;
}

static uint16_t mouse_value(void *context, unsigned selector) {
    return ((FakeDevices *)context)->mouse[selector];
}

static void mouse_set_position(void *context, uint16_t x, uint16_t y) {
    FakeDevices *devices = (FakeDevices *)context;
    devices->set_x = x;
    devices->set_y = y;
}

static int require_input(const SrLevelInput *input, int steer, int throttle, int jump) {
    return input->steering == steer && input->throttle == throttle && input->jump == jump;
}

static int test_full_demo(const char *path, const SrInputDevices *devices) {
    FILE *stream = fopen(path, "rb");
    uint8_t *bytes;
    long size;
    size_t index;
    unsigned throttle[3] = {0};
    unsigned steering[3] = {0};
    unsigned jump[2] = {0};
    SrLevelInput input;

    if (stream == NULL) return 0;
    fseek(stream, 0, SEEK_END);
    size = ftell(stream);
    fseek(stream, 0, SEEK_SET);
    bytes = (uint8_t *)malloc((size_t)size);
    if (bytes == NULL || fread(bytes, 1, (size_t)size, stream) != (size_t)size) {
        fclose(stream);
        free(bytes);
        return 0;
    }
    fclose(stream);

    memset(&input, 0, sizeof(input));
    input.mode = SR_INPUT_DEMO;
    input.demo_record = bytes;
    input.demo_record_size = (size_t)size;
    for (index = 0; index < (size_t)size; ++index) {
        input.road_distance = (uint32_t)(index * UINT32_C(0x666));
        if (!sr_sample_level_input(&input, devices) || input.throttle < -1 || input.throttle > 1 ||
            input.steering < -1 || input.steering > 1 || input.jump > 1) {
            free(bytes);
            return 0;
        }
        ++throttle[input.throttle + 1];
        ++steering[input.steering + 1];
        ++jump[input.jump];
    }
    free(bytes);
    return size == 6398 && throttle[0] == 232 && throttle[1] == 5519 && throttle[2] == 647 &&
        steering[0] == 183 && steering[1] == 5878 && steering[2] == 337 &&
        jump[0] == 5558 && jump[1] == 840;
}

int main(int argc, char **argv) {
    static const uint8_t demo[] = {0x00, 0x05, 0x1a};
    FakeDevices fake = {{0, 150, 40}, {180, 10, 1}, 0, 0};
    SrInputDevices devices = {
        &fake, joystick_axis, joystick_button, mouse_value, mouse_set_position};
    SrLevelInput input;

    memset(&input, 0, sizeof(input));
    input.mode = SR_INPUT_KEYBOARD;
    input.key_flags[3] = 0x80;
    input.key_flags[1] = 0x80;
    input.key_flags[9] = 0x80;
    if (!sr_sample_level_input(&input, &devices) || !require_input(&input, 1, -1, 1)) {
        fputs("keyboard input vector failed\n", stderr);
        return 1;
    }

    memset(&input, 0, sizeof(input));
    input.mode = SR_INPUT_JOYSTICK;
    input.joystick_center_x = 80;
    input.joystick_center_y = 100;
    if (!sr_sample_level_input(&input, &devices) || !require_input(&input, 1, 1, 1)) {
        fputs("joystick input vector failed\n", stderr);
        return 1;
    }

    memset(&input, 0, sizeof(input));
    input.mode = SR_INPUT_MOUSE;
    if (!sr_sample_level_input(&input, &devices) || !require_input(&input, 1, 1, 1) ||
        fake.set_x != 160 || fake.set_y != 10) {
        fputs("mouse input vector failed\n", stderr);
        return 1;
    }

    memset(&input, 0, sizeof(input));
    input.mode = SR_INPUT_DEMO;
    input.road_distance = UINT32_C(0x0ccc);
    input.demo_record = demo;
    input.demo_record_size = sizeof(demo);
    if (!sr_sample_level_input(&input, &devices) || !require_input(&input, 1, 1, 1)) {
        fputs("demo input vector failed\n", stderr);
        return 1;
    }

    if (argc != 2 || !test_full_demo(argv[1], &devices)) {
        fputs("full demo.rec input vector failed\n", stderr);
        return 1;
    }

    puts("Recovered input sampler passed all device modes and all 6398 demo records");
    return 0;
}
