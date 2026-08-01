#include "music.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    DRO_HEADER_BYTES = 26,
    DOS_GAMEPLAY_TICKS = 1702,
    OPL_TICKS_PER_GAMEPLAY_TICK = 5
};

typedef struct OplWrite {
    uint16_t reg;
    uint8_t value;
} OplWrite;

typedef struct DroOracle {
    OplWrite *writes;
    size_t count;
} DroOracle;

typedef struct EffectiveOplLog {
    const DroOracle *oracle;
    uint8_t cache[512];
    size_t cursor;
    size_t actual_count;
    int capturing;
    int mismatch;
} EffectiveOplLog;

static uint16_t read_u16(const uint8_t *bytes) {
    return (uint16_t)(bytes[0] | ((uint16_t)bytes[1] << 8));
}

static uint32_t read_u32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] |
        ((uint32_t)bytes[1] << 8) |
        ((uint32_t)bytes[2] << 16) |
        ((uint32_t)bytes[3] << 24);
}

static uint8_t *read_file(const char *path, size_t *size) {
    FILE *stream = fopen(path, "rb");
    long length;
    uint8_t *bytes;
    if (stream == 0) return 0;
    if (fseek(stream, 0, SEEK_END) != 0 || (length = ftell(stream)) < 0 ||
        fseek(stream, 0, SEEK_SET) != 0) {
        fclose(stream);
        return 0;
    }
    bytes = (uint8_t *)malloc((size_t)length);
    if (bytes == 0 || fread(bytes, 1, (size_t)length, stream) != (size_t)length) {
        free(bytes);
        fclose(stream);
        return 0;
    }
    fclose(stream);
    *size = (size_t)length;
    return bytes;
}

static int load_dro_oracle(const char *path, DroOracle *oracle) {
    uint8_t *bytes;
    size_t size;
    uint32_t command_count;
    uint8_t delay_256;
    uint8_t delay_shift_8;
    uint8_t table_size;
    const uint8_t *table;
    const uint8_t *commands;
    size_t command;
    size_t write_count = 0;
    size_t cursor = 0;

    memset(oracle, 0, sizeof(*oracle));
    bytes = read_file(path, &size);
    if (bytes == 0 || size < DRO_HEADER_BYTES ||
        memcmp(bytes, "DBRAWOPL", 8) != 0 ||
        read_u16(bytes + 8) != 2 || read_u16(bytes + 10) != 0 ||
        bytes[20] != 0 || bytes[21] != 0 || bytes[22] != 0) {
        free(bytes);
        return 0;
    }
    command_count = read_u32(bytes + 12);
    delay_256 = bytes[23];
    delay_shift_8 = bytes[24];
    table_size = bytes[25];
    if (table_size == 0 || table_size > 127 || delay_256 == delay_shift_8 ||
        size != DRO_HEADER_BYTES + (size_t)table_size + (size_t)command_count * 2u) {
        free(bytes);
        return 0;
    }
    table = bytes + DRO_HEADER_BYTES;
    commands = table + table_size;
    for (command = 0; command < command_count; ++command) {
        uint8_t code = commands[command * 2u];
        if (code != delay_256 && code != delay_shift_8) ++write_count;
    }
    oracle->writes = (OplWrite *)calloc(write_count, sizeof(*oracle->writes));
    if (oracle->writes == 0) {
        free(bytes);
        return 0;
    }
    for (command = 0; command < command_count; ++command) {
        uint8_t code = commands[command * 2u];
        uint8_t value = commands[command * 2u + 1u];
        uint8_t index;
        if (code == delay_256 || code == delay_shift_8) continue;
        index = (uint8_t)(code & 0x7fu);
        if (index >= table_size) {
            free(oracle->writes);
            memset(oracle, 0, sizeof(*oracle));
            free(bytes);
            return 0;
        }
        oracle->writes[cursor].reg = (uint16_t)(table[index] |
            ((uint16_t)(code & 0x80u) << 1));
        oracle->writes[cursor].value = value;
        ++cursor;
    }
    oracle->count = write_count;
    free(bytes);
    return 1;
}

static void compare_effective_write(
    EffectiveOplLog *log,
    uint16_t reg,
    uint8_t value) {
    size_t index = log->actual_count++;
    if (log->cursor >= log->oracle->count) {
        if (!log->mismatch) {
            fprintf(stderr,
                "DOS OPL oracle exhausted at write %zu: native=%03x:%02x\n",
                index, reg, value);
        }
        log->mismatch = 1;
        return;
    }
    if (log->oracle->writes[log->cursor].reg != reg ||
        log->oracle->writes[log->cursor].value != value) {
        if (!log->mismatch) {
            fprintf(stderr,
                "DOS OPL mismatch at write %zu: native=%03x:%02x DOS=%03x:%02x\n",
                index, reg, value,
                log->oracle->writes[log->cursor].reg,
                log->oracle->writes[log->cursor].value);
        }
        log->mismatch = 1;
    }
    ++log->cursor;
}

static int dro_register_supported(uint16_t reg) {
    uint8_t low = (uint8_t)reg;
    uint8_t group = (uint8_t)(low & 0xe0u);
    if (low == 0x01u || low == 0x04u || low == 0x05u || low == 0x08u ||
        low == 0xbdu) return 1;
    if (low >= 0xa0u && low <= 0xc8u && (low & 0x0fu) <= 8u) return 1;
    if (group == 0x20u || group == 0x40u || group == 0x60u ||
        group == 0x80u || group == 0xe0u) {
        uint8_t offset = (uint8_t)(low & 0x1fu);
        return offset < 24u && (offset & 7u) < 6u;
    }
    return 0;
}

/* Match DOSBox-X's DROv2 capture boundary and effective-write filtering. */
static void log_opl_register(void *context, uint8_t reg8, uint8_t value) {
    EffectiveOplLog *log = (EffectiveOplLog *)context;
    uint16_t reg = reg8;
    if (!dro_register_supported(reg)) {
        log->cache[reg] = value;
        return;
    }
    if (!log->capturing) {
        int starts_capture =
            (reg >= 0xb0u && reg <= 0xb8u && (value & 0x20u) != 0) ||
            (reg == 0xbdu && (value & 0x3fu) > 0x20u);
        if (starts_capture) {
            unsigned cache_reg;
            log->capturing = 1;
            for (cache_reg = 0; cache_reg < 256u; ++cache_reg) {
                uint8_t cached = log->cache[cache_reg];
                if (!dro_register_supported((uint16_t)cache_reg)) continue;
                if (cache_reg >= 0xb0u && cache_reg <= 0xb8u) cached &= 0xdfu;
                if (cache_reg == 0xbdu) cached &= 0xe0u;
                if (cached != 0) {
                    compare_effective_write(log, (uint16_t)cache_reg, cached);
                }
            }
            compare_effective_write(log, reg, value);
        }
        log->cache[reg] = value;
        return;
    }
    if (log->cache[reg] == value) return;
    compare_effective_write(log, reg, value);
    log->cache[reg] = value;
}

int main(int argc, char **argv) {
    uint8_t *music_bytes;
    size_t music_size;
    SrMusicArchive archive;
    SrOplPlayer player;
    SrOplHooks hooks;
    DroOracle oracle;
    EffectiveOplLog log;
    unsigned tick;
    int ok = 0;

    if (argc != 3 || !load_dro_oracle(argv[2], &oracle)) return 1;
    music_bytes = read_file(argv[1], &music_size);
    if (music_bytes == 0 ||
        sr_load_music_archive(music_bytes, music_size, &archive) !=
            SR_MUSIC_ARCHIVE_OK) {
        free(music_bytes);
        free(oracle.writes);
        return 1;
    }
    free(music_bytes);
    memset(&log, 0, sizeof(log));
    log.oracle = &oracle;
    hooks.context = &log;
    hooks.write_register = log_opl_register;

    sr_opl_player_init(&player, &hooks);
    if (!sr_opl_load_track(&player, &archive, 0, 0)) {
        goto done;
    }
    for (tick = 0;
         tick < DOS_GAMEPLAY_TICKS * OPL_TICKS_PER_GAMEPLAY_TICK;
         ++tick) {
        if (sr_opl_tick(&player) != SR_OPL_TICK_OK) goto done;
    }
    ok = log.capturing && !log.mismatch &&
        log.cursor == oracle.count && log.actual_count == oracle.count;
    if (!ok) {
        fprintf(stderr,
            "OPL oracle summary: native=%zu consumed=%zu DOS=%zu capturing=%d mismatch=%d\n",
            log.actual_count, log.cursor, oracle.count,
            log.capturing, log.mismatch);
    }
    else {
        printf("DOS OPL oracle matched %zu effective register writes\n", oracle.count);
    }

done:
    sr_free_music_archive(&archive);
    free(oracle.writes);
    return ok ? 0 : 1;
}
