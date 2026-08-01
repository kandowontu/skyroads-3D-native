#include "dashboard.h"
#include "road_archive.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *read_file(const char *path, size_t *size) {
    FILE *stream = fopen(path, "rb");
    long length;
    uint8_t *bytes;
    if (stream == 0) return 0;
    fseek(stream, 0, SEEK_END); length = ftell(stream); fseek(stream, 0, SEEK_SET);
    if (length < 0) { fclose(stream); return 0; }
    bytes = (uint8_t *)malloc((size_t)length);
    if (bytes == 0 || fread(bytes, 1, (size_t)length, stream) != (size_t)length) {
        fclose(stream); free(bytes); return 0;
    }
    fclose(stream); *size = (size_t)length; return bytes;
}

static uint64_t hash_bytes(const uint8_t *bytes, size_t size) {
    uint64_t hash = UINT64_C(1469598103934665603);
    size_t index;
    for (index = 0; index < size; ++index) {
        hash ^= bytes[index]; hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static int load_display(const char *path, size_t count, SrDisplayTable *table) {
    uint8_t *bytes;
    size_t size;
    int ok;
    bytes = read_file(path, &size);
    if (bytes == 0) return 0;
    ok = sr_load_display_table(bytes, size, count, table);
    free(bytes);
    return ok;
}

static void sound(void *context, unsigned effect) {
    unsigned *count = (unsigned *)context;
    if (effect == 3) ++*count;
}

static int write_ppm(
    const char *path,
    const uint8_t *framebuffer,
    const SrRoadData *road,
    const SrGraphicsArchive *dashboard,
    const SrGraphicsArchive *world) {
    uint8_t palette[256 * 3] = {0};
    FILE *stream;
    size_t pixel;
    size_t index;
    memcpy(palette, road->palette, sizeof(road->palette));
    memcpy(palette + 0x5cu * 3u, dashboard->palette,
        (size_t)dashboard->palette_count * 3u);
    memcpy(palette + 0x8eu * 3u, world->palette,
        (size_t)world->palette_count * 3u);
    stream = fopen(path, "wb");
    if (stream == 0) return 0;
    fprintf(stream, "P6\n320 200\n255\n");
    for (pixel = 0; pixel < SR_PICTURE_SCREEN_SIZE; ++pixel) {
        uint8_t color = framebuffer[pixel];
        for (index = 0; index < 3; ++index) {
            uint8_t channel = palette[(size_t)color * 3u + index];
            fputc((int)((channel << 2) | (channel >> 4)), stream);
        }
    }
    fclose(stream);
    return 1;
}

int main(int argc, char **argv) {
    uint8_t *exe_bytes, *dash_bytes, *world_bytes, *road_bytes;
    size_t exe_size, dash_size, world_size, road_size;
    SrEmbeddedHud hud;
    SrGraphicsArchive dash;
    SrGraphicsArchive world;
    SrRoadArchive roads;
    SrDisplayTable oxygen, fuel, speed;
    SrDashboardState state;
    SrDashboardInput input;
    SrDashboardHooks hooks;
    uint8_t framebuffer[SR_PICTURE_SCREEN_SIZE];
    uint64_t initial_hash, active_hash, returned_hash;
    unsigned sounds = 0;
    if (argc != 8 && argc != 9) return 1;
    exe_bytes = read_file(argv[1], &exe_size);
    dash_bytes = read_file(argv[2], &dash_size);
    world_bytes = read_file(argv[3], &world_size);
    road_bytes = read_file(argv[4], &road_size);
    if (exe_bytes == 0 || dash_bytes == 0 || world_bytes == 0 || road_bytes == 0 ||
        !sr_load_embedded_hud_from_exe(exe_bytes, exe_size, &hud) ||
        sr_load_vga_graphics_archive(dash_bytes, dash_size, 0x5c, &dash) !=
            SR_GRAPHICS_ARCHIVE_OK ||
        sr_load_vga_graphics_archive(world_bytes, world_size, 0x8e, &world) !=
            SR_GRAPHICS_ARCHIVE_OK ||
        sr_load_road_archive(road_bytes, road_size, &roads) != SR_ROAD_ARCHIVE_OK ||
        !load_display(argv[5], 10, &oxygen) ||
        !load_display(argv[6], 10, &fuel) ||
        !load_display(argv[7], 34, &speed)) return 1;
    free(exe_bytes); free(dash_bytes); free(world_bytes); free(road_bytes);
    if (dash.picture_count < 1 || world.picture_count < 1 || roads.road_count < 1 ||
        !sr_build_gameplay_background_vga(
            framebuffer, &world.pictures[0], &dash.pictures[0]) ||
        !sr_draw_dashboard_gravity_vga(framebuffer, &hud, roads.roads[0].gravity)) return 1;

    sr_dashboard_state_init(&state);
    input.tick_count = 0;
    input.forward_speed = 0;
    input.collision_speed_correction = 0;
    input.oxygen = 30000;
    input.fuel = 30000;
    input.level_result = 0;
    input.road_length_rows = (uint16_t)roads.roads[0].row_count;
    input.jumpmaster = 0;
    input.distance = 0x00030000u;
    hooks.context = &sounds;
    hooks.play_sound = sound;
    if (!sr_update_gameplay_dashboard_vga(framebuffer, &speed, &oxygen, &fuel,
            &hud, &input, &hooks, &state)) return 1;
    initial_hash = hash_bytes(framebuffer, sizeof(framebuffer));

    input.tick_count = 5;
    input.forward_speed = 0x2000;
    input.oxygen = 0;
    input.fuel = 15000;
    input.level_result = 5;
    input.jumpmaster = 1;
    input.distance = ((uint32_t)input.road_length_rows << 15) + 0x00018000u;
    if (!sr_update_gameplay_dashboard_vga(framebuffer, &speed, &oxygen, &fuel,
            &hud, &input, &hooks, &state)) return 1;
    active_hash = hash_bytes(framebuffer, sizeof(framebuffer));

    input.tick_count = 9;
    if (!sr_update_gameplay_dashboard_vga(framebuffer, &speed, &oxygen, &fuel,
            &hud, &input, &hooks, &state)) return 1;
    returned_hash = hash_bytes(framebuffer, sizeof(framebuffer));
    printf("dashboard: initial=%016llx active=%016llx returned=%016llx sounds=%u\n",
        (unsigned long long)initial_hash, (unsigned long long)active_hash,
        (unsigned long long)returned_hash, sounds);
    if (argc == 9 && !write_ppm(
            argv[8], framebuffer, &roads.roads[0], &dash, &world)) return 1;

    if (initial_hash != UINT64_C(0xed2af9a2d93f1e1a) ||
        active_hash != UINT64_C(0x1080a59b392dadd9) ||
        returned_hash != UINT64_C(0xb0308d4a687388ba) || sounds != 1u) return 1;

    sr_free_display_table(&speed);
    sr_free_display_table(&fuel);
    sr_free_display_table(&oxygen);
    sr_free_road_archive(&roads);
    sr_free_graphics_archive(&world);
    sr_free_graphics_archive(&dash);
    return 0;
}
