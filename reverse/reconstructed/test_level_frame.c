#include "dashboard.h"
#include "palette.h"
#include "renderer_vga.h"
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

static int write_ppm(
    const char *path,
    const uint8_t framebuffer[SR_VGA_FRAMEBUFFER_SIZE],
    const uint8_t palette[SR_VGA_PALETTE_BYTES]) {
    FILE *stream = fopen(path, "wb");
    size_t pixel;
    if (stream == 0) return 0;
    fprintf(stream, "P6\n320 200\n255\n");
    for (pixel = 0; pixel < SR_VGA_FRAMEBUFFER_SIZE; ++pixel) {
        size_t color = (size_t)framebuffer[pixel] * 3u;
        size_t channel;
        for (channel = 0; channel < 3; ++channel) {
            uint8_t value = palette[color + channel];
            fputc((int)((value << 2) | (value >> 4)), stream);
        }
    }
    fclose(stream);
    return 1;
}

int main(int argc, char **argv) {
    uint8_t *exe_bytes, *dash_bytes, *world_bytes, *road_bytes, *trek_bytes, *car_bytes;
    size_t exe_size, dash_size, world_size, road_size, trek_size, car_size;
    SrEmbeddedHud hud;
    SrGraphicsArchive dash, world;
    SrRoadArchive roads;
    SrTrekArchive trek;
    SrCarSprites cars;
    SrRendererTables renderer_tables;
    SrVgaRendererState renderer_state;
    SrDisplayTable oxygen, fuel, speed;
    SrGameplayConfig config;
    SrGameplayState gameplay;
    SrRoadFrameParams params;
    SrDashboardState dashboard_state;
    SrDashboardInput input;
    uint8_t background[SR_VGA_FRAMEBUFFER_SIZE];
    uint8_t framebuffer[SR_VGA_FRAMEBUFFER_SIZE];
    uint8_t palette[SR_VGA_PALETTE_BYTES];
    uint64_t palette_hash, initial_hash, moved_hash;

    if (argc != 10 && argc != 11) return 1;
    exe_bytes = read_file(argv[1], &exe_size);
    dash_bytes = read_file(argv[2], &dash_size);
    world_bytes = read_file(argv[3], &world_size);
    road_bytes = read_file(argv[4], &road_size);
    trek_bytes = read_file(argv[5], &trek_size);
    car_bytes = read_file(argv[6], &car_size);
    if (exe_bytes == 0 || dash_bytes == 0 || world_bytes == 0 || road_bytes == 0 ||
        trek_bytes == 0 || car_bytes == 0 ||
        !sr_load_embedded_hud_from_exe(exe_bytes, exe_size, &hud) ||
        !sr_load_renderer_tables_from_exe(exe_bytes, exe_size, &renderer_tables) ||
        sr_load_vga_graphics_archive(dash_bytes, dash_size, SR_DASHBOARD_PALETTE_BASE,
            &dash) != SR_GRAPHICS_ARCHIVE_OK ||
        sr_load_vga_graphics_archive(world_bytes, world_size, SR_WORLD_PALETTE_BASE,
            &world) != SR_GRAPHICS_ARCHIVE_OK ||
        sr_load_road_archive(road_bytes, road_size, &roads) != SR_ROAD_ARCHIVE_OK ||
        sr_load_trek_archive(trek_bytes, trek_size, &trek) != SR_TREK_ARCHIVE_OK ||
        !sr_load_car_sprites(car_bytes, car_size, &cars) ||
        !load_display(argv[7], 10, &oxygen) ||
        !load_display(argv[8], 10, &fuel) ||
        !load_display(argv[9], 34, &speed)) return 1;
    free(exe_bytes); free(dash_bytes); free(world_bytes);
    free(road_bytes); free(trek_bytes); free(car_bytes);
    if (dash.picture_count < 1 || world.picture_count < 1 || roads.road_count < 1 ||
        !sr_build_gameplay_palette_vga(palette, &roads.roads[0], &cars, &dash, &world) ||
        !sr_build_gameplay_background_vga(
            background, &world.pictures[0], &dash.pictures[0]) ||
        !sr_draw_dashboard_gravity_vga(background, &hud, roads.roads[0].gravity)) return 1;
    palette_hash = hash_bytes(palette, sizeof(palette));
    memcpy(framebuffer, background, sizeof(framebuffer));
    sr_vga_renderer_state_init(&renderer_state);

    config.road_gravity = roads.roads[0].gravity;
    config.road_oxygen = roads.roads[0].oxygen;
    config.road_fuel = roads.roads[0].fuel;
    config.road_length_rows = (uint16_t)roads.roads[0].row_count;
    config.collision_resolution_enabled = 1;
    config.input_lock_active = 0;
    sr_gameplay_init(&gameplay, &config);
    sr_dashboard_state_init(&dashboard_state);

    sr_prepare_road_frame(
        roads.roads[0].cells, roads.roads[0].row_count, &gameplay, 0, 0, &params);
    input.tick_count = 0;
    input.forward_speed = gameplay.forward_speed;
    input.collision_speed_correction = gameplay.collision_speed_correction;
    input.oxygen = gameplay.oxygen;
    input.fuel = gameplay.fuel;
    input.level_result = gameplay.level_result;
    input.road_length_rows = config.road_length_rows;
    input.jumpmaster = gameplay.collision_adjusted;
    input.distance = gameplay.position.distance;
    if (!sr_restore_road_viewport_vga(framebuffer, background) ||
        !sr_draw_road_scene_vga(&trek, roads.roads[0].cells,
            roads.roads[0].row_count, &params, &cars, &renderer_tables,
            &renderer_state, background, framebuffer)) return 1;
    {
        size_t pixel;
        for (pixel = 0; pixel < sizeof(framebuffer); ++pixel) {
            if ((pixel < 0x2800u || pixel >= 0xac80u) &&
                framebuffer[pixel] != background[pixel]) return 1;
        }
    }
    if (!sr_update_gameplay_dashboard_vga(framebuffer, &speed, &oxygen, &fuel,
            &hud, &input, 0, &dashboard_state)) return 1;
    initial_hash = hash_bytes(framebuffer, sizeof(framebuffer));

    gameplay.position.distance = 0x00068000u;
    gameplay.position.horizontal_position = 0x9000u;
    gameplay.position.height = 0x3100u;
    gameplay.vertical_velocity = 0x0200;
    gameplay.forward_speed = 0x2200;
    gameplay.fuel = 15000;
    gameplay.oxygen = 6000;
    gameplay.collision_adjusted = 1;
    sr_prepare_road_frame(
        roads.roads[0].cells, roads.roads[0].row_count, &gameplay, 5, 0, &params);
    input.tick_count = 5;
    input.forward_speed = gameplay.forward_speed;
    input.collision_speed_correction = gameplay.collision_speed_correction;
    input.oxygen = gameplay.oxygen;
    input.fuel = gameplay.fuel;
    input.level_result = gameplay.level_result;
    input.jumpmaster = gameplay.collision_adjusted;
    input.distance = gameplay.position.distance;
    if (!sr_restore_road_viewport_vga(framebuffer, background) ||
        !sr_draw_road_scene_vga(&trek, roads.roads[0].cells,
            roads.roads[0].row_count, &params, &cars, &renderer_tables,
            &renderer_state, background, framebuffer) ||
        !sr_update_gameplay_dashboard_vga(framebuffer, &speed, &oxygen, &fuel,
            &hud, &input, 0, &dashboard_state)) return 1;
    moved_hash = hash_bytes(framebuffer, sizeof(framebuffer));
    printf("level frame: palette=%016llx initial=%016llx moved=%016llx\n",
        (unsigned long long)palette_hash, (unsigned long long)initial_hash,
        (unsigned long long)moved_hash);
    if (palette_hash != UINT64_C(0x88d3c64eca0d7ffd) ||
        initial_hash != UINT64_C(0xbbbbbeac071ffcc5) ||
        moved_hash != UINT64_C(0xd39f6dfba73cf1ab)) return 1;
    if (argc == 11 && !write_ppm(argv[10], framebuffer, palette)) return 1;

    sr_free_display_table(&speed);
    sr_free_display_table(&fuel);
    sr_free_display_table(&oxygen);
    sr_free_car_sprites(&cars);
    sr_free_trek_archive(&trek);
    sr_free_road_archive(&roads);
    sr_free_graphics_archive(&world);
    sr_free_graphics_archive(&dash);
    return 0;
}
