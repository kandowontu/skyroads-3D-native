#include "music.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct RegisterLog {
    uint64_t hash;
    size_t count;
    uint8_t last_reg;
    uint8_t last_value;
} RegisterLog;

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

static uint64_t fnv1a64_update(uint64_t hash, const uint8_t *bytes, size_t size) {
    size_t index;
    for (index = 0; index < size; ++index) {
        hash ^= bytes[index];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static void log_register(void *context, uint8_t reg, uint8_t value) {
    RegisterLog *log = (RegisterLog *)context;
    uint8_t pair[2];
    pair[0] = reg; pair[1] = value;
    log->hash = fnv1a64_update(log->hash, pair, 2);
    ++log->count;
    log->last_reg = reg;
    log->last_value = value;
}

int main(int argc, char **argv) {
    static const size_t expected_write_count[SR_MUSIC_TRACK_COUNT] = {
        379, 1043, 2025, 1192, 1604, 1357, 967,
        2061, 1344, 2390, 1389, 969, 1038, 1276
    };
    static const uint64_t expected_write_hash[SR_MUSIC_TRACK_COUNT] = {
        UINT64_C(0x637d49b9383507db), UINT64_C(0xa2fdb220aa6642e3),
        UINT64_C(0xb31f149dcc4ac79d), UINT64_C(0x77ec0f20e46d1314),
        UINT64_C(0x96e6e84e8ec88ff8), UINT64_C(0x5450ce9deac2deb1),
        UINT64_C(0x5a8453edd4b4f6e6), UINT64_C(0x783559bdc051f9e3),
        UINT64_C(0xda1fd991febc79e4), UINT64_C(0x89e0fa8db4ff5d94),
        UINT64_C(0x51a2add6e504363b), UINT64_C(0x97b73a8d677ccb95),
        UINT64_C(0x3f29d914edd6cfd4), UINT64_C(0xafa1fccadf96ce09)
    };
    static const uint16_t expected_cursor[SR_MUSIC_TRACK_COUNT] = {
        576, 1248, 2526, 1204, 1342, 1578, 1174,
        2290, 1782, 2694, 1556, 1190, 1188, 1436
    };
    static const uint8_t expected_delay[SR_MUSIC_TRACK_COUNT] = {
        21, 20, 6, 20, 20, 18, 40, 8, 18, 8, 18, 18, 40, 18
    };
    static const uint8_t expected_rhythm[SR_MUSIC_TRACK_COUNT] = {
        0xe3, 0xf0, 0xf0, 0xf0, 0xe0, 0xe1, 0xe0,
        0xe0, 0xe8, 0xe0, 0xe8, 0xe8, 0xe1, 0xe1
    };
    uint8_t *bytes;
    size_t size;
    SrMusicArchive archive;
    SrMusicArchiveError error;
    SrOplPlayer player;
    SrOplHooks hooks;
    RegisterLog log;
    uint64_t combined = UINT64_C(1469598103934665603);
    unsigned track;
    if (argc != 2) return 1;
    bytes = read_file(argv[1], &size);
    if (bytes == 0) return 1;
    error = sr_load_music_archive(bytes, size, &archive);
    free(bytes);
    if (error != SR_MUSIC_ARCHIVE_OK) return 1;
    for (track = 0; track < SR_MUSIC_TRACK_COUNT; ++track) {
        combined = fnv1a64_update(combined,
            archive.tracks[track].bytes, archive.tracks[track].byte_count);
    }
    if (archive.tracks[0].byte_count != 0x301eu ||
        archive.tracks[0].instrument_count != 9 ||
        archive.tracks[0].event_offset != 0x90u ||
        archive.tracks[13].byte_count != 0x1512u ||
        combined != UINT64_C(0x0c9442bcc9983942)) return 1;

    log.hash = UINT64_C(1469598103934665603);
    log.count = 0;
    hooks.context = &log;
    hooks.write_register = log_register;
    sr_opl_player_init(&player, &hooks);
    if (log.count != 63 || player.rhythm_register != 0xe0 ||
        player.current_track != 0xffffu) return 1;

    if (!sr_opl_load_track(&player, &archive, 1, 0) ||
        player.current_track != 1 || player.cursor != 0x80u) return 1;
    log.hash = UINT64_C(1469598103934665603);
    log.count = 0;
    if (sr_opl_tick(&player) != SR_OPL_TICK_OK || player.delay != 83 ||
        player.rhythm_register != 0xf0 || log.count != 32 ||
        log.hash != UINT64_C(0x10c858e84af3a860)) return 1;

    /* A same-track request is a true no-op, including when muted. */
    if (!sr_opl_load_track(&player, &archive, 1, 1) || log.count != 32 ||
        player.current_track != 1) return 1;
    sr_opl_disable_music(&player);
    if (player.current_track != 0xffffu || player.track == 0) return 1;
    if (!sr_opl_load_track(&player, &archive, 2, 1) ||
        player.current_track != 0xffffu) return 1;

    /* Independent 4096-tick oracles cover every event opcode and all tracks. */
    for (track = 0; track < SR_MUSIC_TRACK_COUNT; ++track) {
        unsigned tick;
        sr_opl_player_init(&player, &hooks);
        if (!sr_opl_load_track(&player, &archive, track, 0)) return 1;
        log.hash = UINT64_C(1469598103934665603);
        log.count = 0;
        for (tick = 0; tick < 4096; ++tick) {
            if (sr_opl_tick(&player) != SR_OPL_TICK_OK) return 1;
        }
        if (log.count != expected_write_count[track] ||
            log.hash != expected_write_hash[track] ||
            player.cursor != expected_cursor[track] ||
            player.delay != expected_delay[track] ||
            player.rhythm_register != expected_rhythm[track] ||
            player.signal != (track == 0 ? 99 : 0)) return 1;
    }

    sr_free_music_archive(&archive);
    return 0;
}
