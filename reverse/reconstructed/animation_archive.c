#include "animation_archive.h"

#include <stdlib.h>
#include <string.h>

static uint16_t read_u16(const uint8_t *bytes) {
    return (uint16_t)(bytes[0] | ((uint16_t)bytes[1] << 8));
}

void sr_free_animation_archive(SrAnimationArchive *archive) {
    size_t index;
    if (archive == 0) return;
    for (index = 0; index < archive->frame_count; ++index) {
        sr_free_picture(&archive->frames[index].picture);
    }
    free(archive->frames);
    free(archive->groups);
    memset(archive, 0, sizeof(*archive));
}

static SrAnimationArchiveError append_frame(
    SrAnimationArchive *archive,
    uint16_t group_index,
    SrPicture *picture) {
    SrAnimationFrame *frames = (SrAnimationFrame *)realloc(
        archive->frames, (archive->frame_count + 1u) * sizeof(*frames));
    if (frames == 0) return SR_ANIMATION_ARCHIVE_OUT_OF_MEMORY;
    archive->frames = frames;
    archive->frames[archive->frame_count].group_index = group_index;
    archive->frames[archive->frame_count].picture = *picture;
    ++archive->frame_count;
    return SR_ANIMATION_ARCHIVE_OK;
}

SrAnimationArchiveError sr_load_animation_archive(
    const uint8_t *bytes,
    size_t size,
    SrAnimationArchive *archive) {
    size_t offset;
    size_t palette_bytes;
    size_t mapping_bytes;
    size_t group;
    size_t index;
    if (archive == 0) return SR_ANIMATION_ARCHIVE_BAD_HEADER;
    memset(archive, 0, sizeof(*archive));
    if (bytes == 0 || size < 11u) return SR_ANIMATION_ARCHIVE_TRUNCATED;
    if (memcmp(bytes, "ANIM", 4) != 0) return SR_ANIMATION_ARCHIVE_BAD_HEADER;
    archive->group_count = read_u16(bytes + 4u);
    archive->groups = (SrAnimationGroup *)calloc(
        archive->group_count, sizeof(*archive->groups));
    if (archive->group_count != 0 && archive->groups == 0) {
        return SR_ANIMATION_ARCHIVE_OUT_OF_MEMORY;
    }
    offset = 6u;
    if (memcmp(bytes + offset, "CMAP", 4) != 0) {
        sr_free_animation_archive(archive);
        return SR_ANIMATION_ARCHIVE_BAD_CMAP;
    }
    archive->palette_count = bytes[offset + 4u];
    palette_bytes = (size_t)archive->palette_count * 3u;
    mapping_bytes = (size_t)archive->palette_count * 2u;
    if (palette_bytes > size - offset - 5u ||
        mapping_bytes > size - offset - 5u - palette_bytes) {
        sr_free_animation_archive(archive);
        return SR_ANIMATION_ARCHIVE_TRUNCATED;
    }
    memcpy(archive->palette, bytes + offset + 5u, palette_bytes);
    offset += 5u + palette_bytes;
    for (index = 0; index < archive->palette_count; ++index) {
        archive->ega_color_pairs[index] = read_u16(bytes + offset + index * 2u);
    }
    offset += mapping_bytes;

    for (group = 0; group < archive->group_count; ++group) {
        uint16_t frame_count;
        uint16_t frame;
        if (size - offset < 2u) {
            sr_free_animation_archive(archive);
            return SR_ANIMATION_ARCHIVE_TRUNCATED;
        }
        frame_count = read_u16(bytes + offset);
        offset += 2u;
        archive->groups[group].first_frame = archive->frame_count;
        archive->groups[group].frame_count = frame_count;
        for (frame = 0; frame < frame_count; ++frame) {
            SrPicture picture;
            size_t consumed;
            SrGraphicsArchiveError picture_error = sr_load_vga_picture(
                bytes + offset, size - offset, 0, &picture, &consumed);
            SrAnimationArchiveError append_error;
            if (picture_error != SR_GRAPHICS_ARCHIVE_OK) {
                sr_free_animation_archive(archive);
                return picture_error == SR_GRAPHICS_ARCHIVE_OUT_OF_MEMORY
                    ? SR_ANIMATION_ARCHIVE_OUT_OF_MEMORY
                    : SR_ANIMATION_ARCHIVE_BAD_PICTURE;
            }
            append_error = append_frame(archive, (uint16_t)group, &picture);
            if (append_error != SR_ANIMATION_ARCHIVE_OK) {
                sr_free_picture(&picture);
                sr_free_animation_archive(archive);
                return append_error;
            }
            offset += consumed;
        }
    }
    if (offset != size) {
        sr_free_animation_archive(archive);
        return SR_ANIMATION_ARCHIVE_BAD_PICTURE;
    }
    return SR_ANIMATION_ARCHIVE_OK;
}
