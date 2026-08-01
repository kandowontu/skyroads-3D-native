#include "dashboard.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

enum {
    VGA_DISPLAY_NORMAL_1 = 0x5c,
    VGA_DISPLAY_NORMAL_2 = 0x5d,
    VGA_DISPLAY_ALTERNATE_1 = 0x5e,
    VGA_DISPLAY_ALTERNATE_2 = 0x5f,
    VGA_PROGRESS_COLOR = 0x60,
    VGA_HUD_COLOR_1 = 0x61,
    VGA_HUD_COLOR_2 = 0x62,
    VGA_WARNING_COLOR_1 = 0x63,
    VGA_WARNING_COLOR_2 = 0x64
};

static int draw_binary_pixels(
    uint8_t framebuffer[SR_PICTURE_SCREEN_SIZE],
    const uint8_t *pixels,
    uint16_t screen_offset,
    uint8_t width,
    uint8_t height,
    uint8_t color_one,
    uint8_t color_two) {
    size_t row;
    size_t column;
    if (framebuffer == 0 || pixels == 0) return 0;
    for (row = 0; row < height; ++row) {
        size_t destination = (size_t)screen_offset + row * SR_PICTURE_SCREEN_WIDTH;
        if (destination > SR_PICTURE_SCREEN_SIZE ||
            width > SR_PICTURE_SCREEN_SIZE - destination) return 0;
        for (column = 0; column < width; ++column) {
            uint8_t pixel = pixels[row * width + column];
            if (pixel == 1) framebuffer[destination + column] = color_one;
            else if (pixel != 0) framebuffer[destination + column] = color_two;
        }
    }
    return 1;
}

static int draw_binary_pixels_opaque(
    uint8_t framebuffer[SR_PICTURE_SCREEN_SIZE],
    const uint8_t *pixels,
    uint16_t screen_offset,
    uint8_t width,
    uint8_t height,
    uint8_t color_one,
    uint8_t color_two) {
    size_t row;
    size_t column;
    if (framebuffer == 0 || pixels == 0) return 0;
    for (row = 0; row < height; ++row) {
        size_t destination = (size_t)screen_offset + row * SR_PICTURE_SCREEN_WIDTH;
        if (destination > SR_PICTURE_SCREEN_SIZE ||
            width > SR_PICTURE_SCREEN_SIZE - destination) return 0;
        for (column = 0; column < width; ++column) {
            uint8_t pixel = pixels[row * width + column];
            framebuffer[destination + column] = pixel == 0
                ? 0 : (pixel == 1 ? color_one : color_two);
        }
    }
    return 1;
}

void sr_dashboard_state_init(SrDashboardState *state) {
    if (state == 0) return;
    memset(state, 0, sizeof(*state));
    state->previous_jumpmaster = 0xffffu;
}

int sr_render_dashboard_sprite_vga(
    uint8_t framebuffer[SR_PICTURE_SCREEN_SIZE],
    const SrDisplaySprite *sprite,
    int alternate_colors) {
    uint8_t color_one = alternate_colors
        ? VGA_DISPLAY_ALTERNATE_1 : VGA_DISPLAY_NORMAL_1;
    uint8_t color_two = alternate_colors
        ? VGA_DISPLAY_ALTERNATE_2 : VGA_DISPLAY_NORMAL_2;
    if (sprite == 0 || sprite->pixel_count !=
            (size_t)sprite->width * sprite->height) return 0;
    return draw_binary_pixels(framebuffer, sprite->pixels, sprite->screen_offset,
        sprite->width, sprite->height, color_one, color_two);
}

int sr_build_gameplay_background_vga(
    uint8_t framebuffer[SR_PICTURE_SCREEN_SIZE],
    const SrPicture *world,
    const SrPicture *dashboard) {
    SrPictureBlitter blitter;
    if (framebuffer == 0) return 0;
    memset(framebuffer, 0, SR_PICTURE_SCREEN_SIZE);
    if (!sr_copy_picture_vga(framebuffer, world)) return 0;
    blitter.destination = framebuffer;
    blitter.destination_size = SR_PICTURE_SCREEN_SIZE;
    blitter.reference = framebuffer;
    blitter.transparent_below = 1;
    blitter.restore_below = 0;
    return sr_draw_picture_composited(&blitter, dashboard);
}

static int draw_number(
    uint8_t framebuffer[SR_PICTURE_SCREEN_SIZE],
    const SrEmbeddedHud *hud,
    uint16_t x,
    uint16_t y,
    uint16_t value,
    uint16_t digit_count) {
    uint16_t index;
    if (hud == 0 || digit_count > 4) return 0;
    for (index = 0; index < digit_count; ++index) {
        uint16_t digit;
        uint16_t destination_x;
        if (value == 0 && index != 0) break;
        digit = (uint16_t)((value / hud->decimal_divisors[index]) % 10u);
        destination_x = (uint16_t)(x + (digit_count - index - 1u) * 5u);
        if (!draw_binary_pixels_opaque(framebuffer, hud->digits[digit],
                (uint16_t)(y * SR_PICTURE_SCREEN_WIDTH + destination_x),
                4, 5, VGA_HUD_COLOR_1, VGA_HUD_COLOR_2)) return 0;
        value = (uint16_t)(value - digit * hud->decimal_divisors[index]);
    }
    return 1;
}

int sr_draw_dashboard_gravity_vga(
    uint8_t framebuffer[SR_PICTURE_SCREEN_SIZE],
    const SrEmbeddedHud *hud,
    uint16_t road_gravity) {
    uint16_t value = (uint16_t)((uint16_t)(road_gravity - 3u) * 100u);
    return draw_number(framebuffer, hud, 0x60, 0x9c, value, 4);
}

static uint16_t resource_level(uint16_t value) {
    uint16_t level = (uint16_t)((value + 0x0bb7u) / 0x0bb8u);
    return level > 10u ? 10u : level;
}

static int draw_display_changes(
    uint8_t framebuffer[SR_PICTURE_SCREEN_SIZE],
    const SrDisplayTable *table,
    uint16_t previous,
    uint16_t current) {
    uint16_t index = previous < current ? previous : current;
    uint16_t end = previous < current ? current : previous;
    int alternate = current > previous;
    while (index < end) {
        SrDisplaySprite sprite;
        if (!sr_get_display_sprite(table, index, &sprite) ||
            !sr_render_dashboard_sprite_vga(framebuffer, &sprite, alternate)) return 0;
        ++index;
    }
    return 1;
}

static void swap_rectangle_colors(
    uint8_t framebuffer[SR_PICTURE_SCREEN_SIZE],
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t height,
    uint8_t first,
    uint8_t second) {
    uint16_t row;
    uint16_t column;
    for (row = 0; row < height; ++row) {
        for (column = 0; column < width; ++column) {
            uint8_t *pixel = &framebuffer[(size_t)(y + row) *
                SR_PICTURE_SCREEN_WIDTH + x + column];
            if (*pixel == first) *pixel = second;
            else if (*pixel == second) *pixel = first;
        }
    }
}

static void fill_progress_column(
    uint8_t framebuffer[SR_PICTURE_SCREEN_SIZE],
    uint16_t progress) {
    uint16_t x = (uint16_t)(progress + 0x2au);
    int y = 0x8f;
    uint8_t color = framebuffer[(size_t)y * SR_PICTURE_SCREEN_WIDTH + x];
    while (y >= 0 && framebuffer[(size_t)y * SR_PICTURE_SCREEN_WIDTH + x] == color) {
        --y;
    }
    ++y;
    while (y < SR_PICTURE_SCREEN_HEIGHT &&
           framebuffer[(size_t)y * SR_PICTURE_SCREEN_WIDTH + x] == color) {
        framebuffer[(size_t)y * SR_PICTURE_SCREEN_WIDTH + x] = VGA_PROGRESS_COLOR;
        ++y;
    }
}

static uint16_t progress_level(const SrDashboardInput *input) {
    const uint32_t start = 0x00030000u;
    uint32_t total = (uint32_t)input->road_length_rows << 16;
    uint32_t step;
    uint32_t position;
    uint32_t level;
    if (total <= start || input->distance <= start) return 0;
    step = (total - start) / 30u;
    if (step == 0) return 0;
    position = input->distance - start;
    level = position / step;
    return (uint16_t)(level > 29u ? 29u : level);
}

int sr_update_gameplay_dashboard_vga(
    uint8_t framebuffer[SR_PICTURE_SCREEN_SIZE],
    const SrDisplayTable *speed,
    const SrDisplayTable *oxygen,
    const SrDisplayTable *fuel,
    const SrEmbeddedHud *hud,
    const SrDashboardInput *input,
    const SrDashboardHooks *hooks,
    SrDashboardState *state) {
    int32_t speed_value;
    uint16_t speed_level;
    uint16_t oxygen_level;
    uint16_t fuel_level;
    uint16_t phase;
    uint16_t progress;
    uint16_t index;
    if (framebuffer == 0 || speed == 0 || oxygen == 0 || fuel == 0 ||
        hud == 0 || input == 0 || state == 0) return 0;

    phase = (uint16_t)((input->tick_count % 9u) > 4u);
    speed_value = input->forward_speed - input->collision_speed_correction;
    speed_level = speed_value <= 0 ? 0u : (uint16_t)(speed_value / 0x0141);
    if (speed_level > 0x22u) speed_level = 0x22u;
    if (!draw_display_changes(framebuffer, speed,
            state->previous_speed_level, speed_level)) return 0;
    state->previous_speed_level = speed_level;

    oxygen_level = resource_level(input->oxygen);
    if (!draw_display_changes(framebuffer, oxygen,
            state->previous_oxygen_level, oxygen_level)) return 0;
    state->previous_oxygen_level = oxygen_level;
    if (input->level_result == 5u && phase != state->previous_warning_phase) {
        swap_rectangle_colors(framebuffer, 0xa0, 0xa1, 7, 7,
            VGA_WARNING_COLOR_1, VGA_WARNING_COLOR_2);
        if (phase != 0 && hooks != 0 && hooks->play_sound != 0) {
            hooks->play_sound(hooks->context, 3);
        }
    }

    fuel_level = resource_level(input->fuel);
    if (!draw_display_changes(framebuffer, fuel,
            state->previous_fuel_level, fuel_level)) return 0;
    state->previous_fuel_level = fuel_level;
    if (input->level_result == 4u && phase != state->previous_warning_phase) {
        swap_rectangle_colors(framebuffer, 0x9b, 0xa9, 0x10, 5,
            VGA_WARNING_COLOR_1, VGA_WARNING_COLOR_2);
        if (phase != 0 && hooks != 0 && hooks->play_sound != 0) {
            hooks->play_sound(hooks->context, 3);
        }
    }

    progress = progress_level(input);
    for (index = state->previous_progress_column; index < progress; ++index) {
        fill_progress_column(framebuffer, index);
    }
    state->previous_progress_column = progress;

    if (input->jumpmaster != state->previous_jumpmaster) {
        if (input->jumpmaster >= 2u || !draw_binary_pixels_opaque(
                framebuffer, hud->jumpmaster[input->jumpmaster],
                (uint16_t)(0x9cu * SR_PICTURE_SCREEN_WIDTH + 0xcbu),
                0x1a, 5, VGA_HUD_COLOR_1, VGA_HUD_COLOR_2)) return 0;
        state->previous_jumpmaster = input->jumpmaster;
    }
    state->previous_warning_phase = phase;
    return 1;
}
