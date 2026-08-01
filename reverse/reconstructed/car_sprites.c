#include "car_sprites.h"

#include <stdlib.h>
#include <string.h>

#include "graphics_archive.h"

void sr_free_car_sprites(SrCarSprites *sprites) {
    if (sprites == 0) return;
    free(sprites->pixels);
    memset(sprites, 0, sizeof(*sprites));
}

int sr_load_car_sprites(const uint8_t *bytes, size_t size, SrCarSprites *sprites) {
    SrGraphicsArchive archive;
    SrPicture *picture;

    if (sprites == 0) return 0;
    memset(sprites, 0, sizeof(*sprites));
    if (sr_load_vga_graphics_archive(bytes, size, 0x48u, &archive) !=
            SR_GRAPHICS_ARCHIVE_OK ||
        archive.palette_count != 20 || archive.picture_count != 1) {
        sr_free_graphics_archive(&archive);
        return 0;
    }
    picture = &archive.pictures[0];
    if (picture->width != 24 || picture->height != 2310 ||
        picture->pixel_count % 0x02d0u != 0) {
        sr_free_graphics_archive(&archive);
        return 0;
    }
    memcpy(sprites->palette, archive.palette, sizeof(sprites->palette));
    sprites->pixels = picture->pixels;
    sprites->pixel_count = picture->pixel_count;
    sprites->frame_count = picture->pixel_count / 0x02d0u;
    picture->pixels = 0;
    sr_free_graphics_archive(&archive);
    return 1;
}
