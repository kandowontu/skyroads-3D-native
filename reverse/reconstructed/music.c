#include "music.h"

#include "lzs.h"

#include <stdlib.h>
#include <string.h>

static const uint8_t operator_register_base[11] = {
    0x20, 0x40, 0x60, 0x80, 0xe0,
    0x20, 0x40, 0x60, 0x80, 0xe0, 0xc0
};
static const uint8_t operator_one_offset[SR_OPL_CHANNEL_COUNT] = {
    0x00, 0x01, 0x02, 0x08, 0x09, 0x0a, 0x10, 0x14, 0x12, 0x15, 0x11
};
static const uint8_t operator_two_offset[SR_OPL_CHANNEL_COUNT] = {
    0x03, 0x04, 0x05, 0x0b, 0x0c, 0x0d, 0x13, 0xff, 0xff, 0xff, 0xff
};
static const uint8_t channel_register_offset[SR_OPL_CHANNEL_COUNT] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0xff, 0x08, 0xff
};
static const uint8_t volume_attenuation[31] = {
    0x3f, 0x14, 0x10, 0x0e, 0x0c, 0x0a, 0x09, 0x08,
    0x07, 0x06, 0x06, 0x05, 0x05, 0x04, 0x04, 0x04,
    0x04, 0x04, 0x03, 0x03, 0x03, 0x03, 0x02, 0x02,
    0x02, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00
};
static const uint8_t note_low[12] = {
    0xac, 0xb6, 0xc1, 0xcd, 0xd9, 0xe6, 0xf3, 0x02, 0x11, 0x22, 0x33, 0x45
};
static const uint8_t note_high[12] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x01, 0x01, 0x01, 0x01
};
static const uint8_t initial_percussion_instruments[4][11] = {
    {0x0c, 0x00, 0xf8, 0xb5, 0x00, 0x00, 0x00, 0xd6, 0x4f, 0x00, 0x01},
    {0x04, 0x00, 0xf7, 0xb5, 0x00, 0x00, 0x00, 0xd6, 0x4f, 0x00, 0x01},
    {0x01, 0x00, 0xf5, 0xb5, 0x00, 0x00, 0x00, 0xd6, 0x4f, 0x00, 0x01},
    {0x01, 0x00, 0xf7, 0xb5, 0x00, 0x4e, 0x00, 0x10, 0x00, 0x00, 0x01}
};
static const uint8_t silent_events[] = {
    0x06, 0x00, /* remember the loop cursor */
    0x00, 0xff, /* wait 255 ticks */
    0x05, 0x00  /* rewind */
};
static const SrMusicTrack silent_track = {
    (uint8_t *)silent_events, sizeof(silent_events), 0, 0
};

static uint16_t read_u16(const uint8_t *bytes) {
    return (uint16_t)(bytes[0] | ((uint16_t)bytes[1] << 8));
}

static void write_register(SrOplPlayer *player, uint8_t reg, uint8_t value) {
    if (player->hooks.write_register != 0) {
        player->hooks.write_register(player->hooks.context, reg, value);
    }
}

void sr_free_music_archive(SrMusicArchive *archive) {
    unsigned track;
    if (archive == 0) return;
    for (track = 0; track < SR_MUSIC_TRACK_COUNT; ++track) {
        free(archive->tracks[track].bytes);
    }
    memset(archive, 0, sizeof(*archive));
}

SrMusicArchiveError sr_load_music_archive(
    const uint8_t *bytes,
    size_t size,
    SrMusicArchive *archive) {
    unsigned track;
    uint16_t previous_offset = 0;
    if (archive == 0) return SR_MUSIC_ARCHIVE_BAD_DIRECTORY;
    memset(archive, 0, sizeof(*archive));
    if (bytes == 0 || size < SR_MUSIC_TRACK_COUNT * 6u) {
        return SR_MUSIC_ARCHIVE_BAD_DIRECTORY;
    }
    for (track = 0; track < SR_MUSIC_TRACK_COUNT; ++track) {
        uint16_t offset = read_u16(bytes + track * 6u);
        uint16_t instrument_count = read_u16(bytes + track * 6u + 2u);
        uint16_t output_size = read_u16(bytes + track * 6u + 4u);
        uint16_t next_offset = track + 1u < SR_MUSIC_TRACK_COUNT
            ? read_u16(bytes + (track + 1u) * 6u)
            : (uint16_t)size;
        SrLzsResult decoded;
        uint8_t *output;
        size_t event_offset = (size_t)instrument_count * SR_OPL_INSTRUMENT_SIZE;
        if (offset < SR_MUSIC_TRACK_COUNT * 6u || offset >= size ||
            offset <= previous_offset || next_offset <= offset || next_offset > size ||
            output_size == 0 || output_size >= 0x3e80u ||
            event_offset >= output_size || (output_size - event_offset) % 2u != 0) {
            sr_free_music_archive(archive);
            return SR_MUSIC_ARCHIVE_BAD_TRACK;
        }
        output = (uint8_t *)malloc(output_size);
        if (output == 0) {
            sr_free_music_archive(archive);
            return SR_MUSIC_ARCHIVE_OUT_OF_MEMORY;
        }
        decoded = sr_lzs_decompress(
            bytes + offset, next_offset - offset, output, output_size);
        if (decoded.error != SR_LZS_OK) {
            free(output);
            sr_free_music_archive(archive);
            return SR_MUSIC_ARCHIVE_DECOMPRESSION_FAILED;
        }
        archive->tracks[track].bytes = output;
        archive->tracks[track].byte_count = output_size;
        archive->tracks[track].instrument_count = instrument_count;
        archive->tracks[track].event_offset = event_offset;
        previous_offset = offset;
    }
    return SR_MUSIC_ARCHIVE_OK;
}

static void key_off(SrOplPlayer *player, unsigned channel) {
    if (channel < 6u) {
        write_register(player, (uint8_t)(0xb0u + channel), 0);
    }
    else if (channel < SR_OPL_CHANNEL_COUNT) {
        player->rhythm_register = (uint8_t)(player->rhythm_register &
            (uint8_t)~(0x10u >> (channel - 6u)));
        write_register(player, 0xbd, player->rhythm_register);
    }
}

static void program_instrument_bytes(
    SrOplPlayer *player,
    unsigned channel,
    const uint8_t *instrument,
    uint8_t instrument_index) {
    unsigned index;
    key_off(player, channel);
    player->channel_instrument[channel] = instrument_index;
    for (index = 0; index < 5u; ++index) {
        write_register(player,
            (uint8_t)(operator_one_offset[channel] + operator_register_base[index]),
            instrument[index]);
    }
    if (operator_two_offset[channel] != 0xffu) {
        for (index = 5; index < 10u; ++index) {
            write_register(player,
                (uint8_t)(operator_two_offset[channel] + operator_register_base[index]),
                instrument[index]);
        }
    }
    if (channel_register_offset[channel] != 0xffu) {
        write_register(player,
            (uint8_t)(channel_register_offset[channel] + 0xc0u),
            instrument[10]);
    }
}

void sr_opl_silence(SrOplPlayer *player) {
    unsigned reg;
    int channel;
    if (player == 0) return;
    player->track = &silent_track;
    player->cursor = 0;
    player->rhythm_register = 0xe0;
    for (reg = 0x40; reg <= 0x55; ++reg) {
        write_register(player, (uint8_t)reg, 0x3f);
    }
    for (channel = 7; channel >= 0; --channel) {
        key_off(player, (unsigned)channel);
    }
}

void sr_opl_player_init(SrOplPlayer *player, const SrOplHooks *hooks) {
    unsigned channel;
    if (player == 0) return;
    memset(player, 0, sizeof(*player));
    player->current_track = 0xffffu;
    if (hooks != 0) player->hooks = *hooks;
    sr_opl_silence(player);
    write_register(player, 0x01, 0x20);
    write_register(player, 0x08, 0x00);
    write_register(player, 0xbd, 0xe0);
    for (channel = 7; channel < SR_OPL_CHANNEL_COUNT; ++channel) {
        program_instrument_bytes(player, channel,
            initial_percussion_instruments[channel - 7u], 0);
    }
    write_register(player, 0xa8, 0xac);
    write_register(player, 0xb8, 0x0c);
    write_register(player, 0xa7, 0x02);
    write_register(player, 0xb7, 0x0d);
}

int sr_opl_load_track(
    SrOplPlayer *player,
    const SrMusicArchive *archive,
    unsigned track,
    int sound_disabled) {
    if (player == 0 || archive == 0 || track >= SR_MUSIC_TRACK_COUNT) return 0;
    if (player->current_track == track) return 1;
    sr_opl_silence(player);
    if (sound_disabled) return 1;
    if (archive->tracks[track].bytes == 0) return 0;
    player->track = &archive->tracks[track];
    player->cursor = player->track->event_offset;
    player->loop_cursor = player->cursor;
    player->signal = 0;
    player->fault = SR_OPL_TICK_OK;
    player->current_track = (uint16_t)track;
    return 1;
}

void sr_opl_disable_music(SrOplPlayer *player) {
    if (player == 0) return;
    sr_opl_silence(player);
    player->current_track = 0xffffu;
}

static SrOplTickResult set_instrument(
    SrOplPlayer *player,
    unsigned channel,
    uint8_t instrument_index) {
    const uint8_t *instrument;
    if (channel >= SR_OPL_CHANNEL_COUNT) return SR_OPL_TICK_BAD_CHANNEL;
    if (player->track == 0 || instrument_index >= player->track->instrument_count) {
        return SR_OPL_TICK_BAD_INSTRUMENT;
    }
    instrument = player->track->bytes +
        (size_t)instrument_index * SR_OPL_INSTRUMENT_SIZE;
    program_instrument_bytes(player, channel, instrument, instrument_index);
    return SR_OPL_TICK_OK;
}

static SrOplTickResult note_on(
    SrOplPlayer *player,
    unsigned channel,
    uint8_t note) {
    if (channel >= SR_OPL_CHANNEL_COUNT) return SR_OPL_TICK_BAD_CHANNEL;
    key_off(player, channel);
    if (channel < 7u) {
        unsigned octave = note / 12u + 2u;
        unsigned pitch = note % 12u;
        write_register(player,
            (uint8_t)(0xa0u + channel_register_offset[channel]), note_low[pitch]);
        write_register(player, (uint8_t)(0xb0u + channel),
            (uint8_t)(note_high[pitch] | (octave << 2u) |
                (channel < 6u ? 0x20u : 0u)));
    }
    if (channel >= 6u) {
        player->rhythm_register = (uint8_t)(player->rhythm_register |
            (0x10u >> (channel - 6u)));
        write_register(player, 0xbd, player->rhythm_register);
    }
    return SR_OPL_TICK_OK;
}

static void apply_volume_operator(
    SrOplPlayer *player,
    unsigned operator_offset,
    uint8_t instrument_level,
    uint8_t volume) {
    unsigned level = (instrument_level & 0x3fu) + volume_attenuation[volume];
    if (level > 0x3fu) level = 0x3fu;
    write_register(player, (uint8_t)(0x40u + operator_offset),
        (uint8_t)((instrument_level & 0xc0u) | level));
}

static SrOplTickResult set_volume(
    SrOplPlayer *player,
    unsigned channel,
    uint8_t volume) {
    const uint8_t *instrument;
    uint8_t instrument_index;
    if (channel >= SR_OPL_CHANNEL_COUNT) return SR_OPL_TICK_BAD_CHANNEL;
    if (volume >= sizeof(volume_attenuation)) return SR_OPL_TICK_BAD_VOLUME;
    instrument_index = player->channel_instrument[channel];
    if (player->track == 0 || instrument_index >= player->track->instrument_count) {
        return SR_OPL_TICK_BAD_INSTRUMENT;
    }
    instrument = player->track->bytes +
        (size_t)instrument_index * SR_OPL_INSTRUMENT_SIZE;
    if (operator_two_offset[channel] != 0xffu) {
        apply_volume_operator(player, operator_two_offset[channel],
            instrument[6], volume);
        if ((instrument[10] & 1u) == 0) return SR_OPL_TICK_OK;
    }
    apply_volume_operator(player, operator_one_offset[channel],
        instrument[1], volume);
    return SR_OPL_TICK_OK;
}

static SrOplTickResult execute_event(
    SrOplPlayer *player,
    uint8_t low,
    uint8_t operand) {
    unsigned opcode = low & 7u;
    unsigned channel = low >> 4u;
    switch (opcode) {
        case 0:
            player->delay = operand;
            return SR_OPL_TICK_OK;
        case 1:
            return set_instrument(player, channel, operand);
        case 2:
            return note_on(player, channel, operand);
        case 3:
            if (channel >= SR_OPL_CHANNEL_COUNT) return SR_OPL_TICK_BAD_CHANNEL;
            key_off(player, channel);
            return SR_OPL_TICK_OK;
        case 4:
            return set_volume(player, channel, operand);
        case 5:
            if (player->loop_cursor >= player->track->byte_count) {
                return SR_OPL_TICK_BAD_STREAM;
            }
            player->cursor = player->loop_cursor;
            return SR_OPL_TICK_OK;
        case 6:
            player->loop_cursor = player->cursor;
            return SR_OPL_TICK_OK;
        case 7:
            player->signal = operand;
            return SR_OPL_TICK_OK;
        default:
            return SR_OPL_TICK_BAD_STREAM;
    }
}

SrOplTickResult sr_opl_tick(SrOplPlayer *player) {
    size_t event_budget;
    if (player == 0 || player->track == 0) return SR_OPL_TICK_BAD_STREAM;
    if (player->fault != SR_OPL_TICK_OK) return player->fault;
    event_budget = player->track->byte_count / 2u + 1u;
    while (player->delay == 0) {
        uint8_t low;
        uint8_t operand;
        if (event_budget-- == 0 || player->cursor > player->track->byte_count ||
            2u > player->track->byte_count - player->cursor) {
            player->fault = SR_OPL_TICK_BAD_STREAM;
            return player->fault;
        }
        low = player->track->bytes[player->cursor];
        operand = player->track->bytes[player->cursor + 1u];
        player->cursor += 2u;
        player->fault = execute_event(player, low, operand);
        if (player->fault != SR_OPL_TICK_OK) return player->fault;
    }
    --player->delay;
    return SR_OPL_TICK_OK;
}
