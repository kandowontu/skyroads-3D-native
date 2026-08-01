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
    fseek(stream, 0, SEEK_END);
    length = ftell(stream);
    fseek(stream, 0, SEEK_SET);
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

int main(int argc, char **argv) {
    uint8_t *exe_bytes, *trek_bytes, *road_bytes, *car_bytes;
    size_t exe_size, trek_size, road_size, car_size;
    SrTrekArchive trek;
    SrRoadArchive roads;
    SrCarSprites cars;
    SrRendererTables tables;
    SrVgaRendererState renderer_state;
    SrGameplayState state = {0};
    SrRoadFrameParams params;
    uint8_t framebuffer[SR_VGA_FRAMEBUFFER_SIZE];
    uint64_t hash;
    uint64_t special_hash;

    if (argc != 5) return 1;
    exe_bytes = read_file(argv[1], &exe_size);
    trek_bytes = read_file(argv[2], &trek_size);
    road_bytes = read_file(argv[3], &road_size);
    car_bytes = read_file(argv[4], &car_size);
    if (exe_bytes == 0 || trek_bytes == 0 || road_bytes == 0 || car_bytes == 0 ||
        !sr_load_renderer_tables_from_exe(exe_bytes, exe_size, &tables) ||
        sr_load_trek_archive(trek_bytes, trek_size, &trek) != SR_TREK_ARCHIVE_OK ||
        sr_load_road_archive(road_bytes, road_size, &roads) != SR_ROAD_ARCHIVE_OK ||
        !sr_load_car_sprites(car_bytes, car_size, &cars)) return 1;
    free(exe_bytes); free(trek_bytes); free(road_bytes); free(car_bytes);
    memset(framebuffer, 0, sizeof(framebuffer));
    sr_vga_renderer_state_init(&renderer_state);
    state.position.distance = 0x00030000u;
    state.position.horizontal_position = 0x8000u;
    state.position.height = 0x2800u;
    sr_prepare_road_frame(
        roads.roads[0].cells, roads.roads[0].row_count, &state, 0, 0, &params);
    if (!sr_draw_road_scene_vga(
            &trek, roads.roads[0].cells, roads.roads[0].row_count,
            &params, &cars, &tables, &renderer_state, framebuffer,
            framebuffer)) return 1;
    hash = hash_bytes(framebuffer, sizeof(framebuffer));
    printf("VGA scene: frames=%zu hash=%016llx\n",
        cars.frame_count, (unsigned long long)hash);
    if (cars.frame_count != 77 ||
        tables.visibility_half_widths[129] != 89 ||
        tables.visibility_half_widths[137] != 158 ||
        hash != UINT64_C(0x57f370208fbbe098)) return 1;

    memset(framebuffer, 0, sizeof(framebuffer));
    memset(&params, 0, sizeof(params));
    sr_vga_renderer_state_init(&renderer_state);
    params.road_phase = 288;
    params.ship_frame = 43;
    params.ship_frame_byte_offset = 43u * 0x02d0u;
    params.ship_height_units = 80;
    params.horizontal_sample = 0x0100;
    if (!sr_draw_road_scene_vga(
            &trek, roads.roads[0].cells, roads.roads[0].row_count,
            &params, &cars, &tables, &renderer_state, framebuffer,
            framebuffer)) return 1;
    special_hash = hash_bytes(framebuffer, sizeof(framebuffer));
    printf("VGA descriptor colors 72/73: hash=%016llx\n",
        (unsigned long long)special_hash);
    if (special_hash != UINT64_C(0x28e8d2009fc24450)) return 1;
    sr_free_car_sprites(&cars);
    sr_free_road_archive(&roads);
    sr_free_trek_archive(&trek);
    return 0;
}
