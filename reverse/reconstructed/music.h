#ifndef SKYROADS_RECOVERED_MUSIC_H
#define SKYROADS_RECOVERED_MUSIC_H

#include <stddef.h>
#include <stdint.h>

enum {
    SR_MUSIC_TRACK_COUNT = 14,
    SR_OPL_CHANNEL_COUNT = 11,
    SR_OPL_INSTRUMENT_SIZE = 16
};

typedef enum SrMusicArchiveError {
    SR_MUSIC_ARCHIVE_OK = 0,
    SR_MUSIC_ARCHIVE_BAD_DIRECTORY,
    SR_MUSIC_ARCHIVE_BAD_TRACK,
    SR_MUSIC_ARCHIVE_OUT_OF_MEMORY,
    SR_MUSIC_ARCHIVE_DECOMPRESSION_FAILED
} SrMusicArchiveError;

typedef struct SrMusicTrack {
    uint8_t *bytes;
    size_t byte_count;
    uint16_t instrument_count;
    size_t event_offset;
} SrMusicTrack;

typedef struct SrMusicArchive {
    SrMusicTrack tracks[SR_MUSIC_TRACK_COUNT];
} SrMusicArchive;

typedef struct SrOplHooks {
    void *context;
    void (*write_register)(void *context, uint8_t reg, uint8_t value);
} SrOplHooks;

typedef enum SrOplTickResult {
    SR_OPL_TICK_OK = 0,
    SR_OPL_TICK_BAD_STREAM,
    SR_OPL_TICK_BAD_CHANNEL,
    SR_OPL_TICK_BAD_INSTRUMENT,
    SR_OPL_TICK_BAD_VOLUME
} SrOplTickResult;

typedef struct SrOplPlayer {
    const SrMusicTrack *track;
    size_t cursor;
    size_t loop_cursor;
    uint16_t current_track;
    uint8_t delay;
    uint8_t rhythm_register;
    uint8_t channel_instrument[SR_OPL_CHANNEL_COUNT];
    uint8_t signal;
    SrOplTickResult fault;
    SrOplHooks hooks;
} SrOplPlayer;

/* Fourteen six-byte directory entries and compressed payloads in muzax.lzs. */
SrMusicArchiveError sr_load_music_archive(
    const uint8_t *bytes,
    size_t size,
    SrMusicArchive *archive);
void sr_free_music_archive(SrMusicArchive *archive);

/* Exact register/state initialization at skyroads.exe 1000:5889/58B1. */
void sr_opl_player_init(SrOplPlayer *player, const SrOplHooks *hooks);
void sr_opl_silence(SrOplPlayer *player);

/* Track selection behavior at 1000:57A8, after archive I/O/decompression. */
int sr_opl_load_track(
    SrOplPlayer *player,
    const SrMusicArchive *archive,
    unsigned track,
    int sound_disabled);
void sr_opl_disable_music(SrOplPlayer *player);

/* One call of the timer-driven event scheduler at 1000:5A39. */
SrOplTickResult sr_opl_tick(SrOplPlayer *player);

#endif
