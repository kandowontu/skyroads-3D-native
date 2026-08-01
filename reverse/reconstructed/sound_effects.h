#ifndef SKYROADS_RECOVERED_SOUND_EFFECTS_H
#define SKYROADS_RECOVERED_SOUND_EFFECTS_H

#include <stddef.h>
#include <stdint.h>

enum {
    SR_SOUND_EFFECT_COUNT = 5,
    SR_PC_SPEAKER_MAX_NOTES = 16
};

typedef struct SrEmbeddedSoundEffects {
    uint16_t notes[SR_SOUND_EFFECT_COUNT][SR_PC_SPEAKER_MAX_NOTES];
    size_t note_counts[SR_SOUND_EFFECT_COUNT];
} SrEmbeddedSoundEffects;

typedef struct SrSampleEffectArchive {
    uint8_t *bytes;
    size_t size;
    uint16_t offsets[SR_SOUND_EFFECT_COUNT + 1];
} SrSampleEffectArchive;

typedef struct SrSampleEffect {
    uint8_t dsp_time_constant;
    const uint8_t *bytes;
    size_t byte_count;
} SrSampleEffect;

typedef struct SrSoundEffectHooks {
    void *context;
    void (*enable_pc_speaker)(void *context);
    void (*start_sample)(
        void *context,
        const uint8_t *bytes,
        size_t byte_count,
        uint8_t dsp_time_constant);
} SrSoundEffectHooks;

typedef struct SrSoundEffectState {
    uint16_t tick_count;
    uint16_t last_effect_tick;
    uint8_t muted;
    uint8_t sampled_backend;
    uint8_t speaker_effect;
    size_t speaker_note_index;
} SrSoundEffectState;

/* DS:00F2..013B: five zero-terminated PC-speaker PIT-divisor streams. */
int sr_load_embedded_sound_effects_from_exe(
    const uint8_t *bytes,
    size_t size,
    SrEmbeddedSoundEffects *effects);

/* The exact offset table and raw sample payloads consumed at 1000:03C2. */
int sr_load_sample_effect_archive(
    const uint8_t *bytes,
    size_t size,
    SrSampleEffectArchive *archive);
void sr_free_sample_effect_archive(SrSampleEffectArchive *archive);
int sr_get_sample_effect(
    const SrSampleEffectArchive *archive,
    unsigned effect,
    SrSampleEffect *sample);

void sr_sound_effect_state_init(
    SrSoundEffectState *state,
    int sampled_backend);
void sr_sound_effect_set_tick(SrSoundEffectState *state, uint16_t tick_count);
int sr_play_sound_effect(
    SrSoundEffectState *state,
    unsigned effect,
    const SrEmbeddedSoundEffects *speaker_effects,
    const SrSampleEffectArchive *sample_effects,
    const SrSoundEffectHooks *hooks);
int sr_sound_effect_is_active(
    const SrSoundEffectState *state,
    const SrEmbeddedSoundEffects *speaker_effects);

/* PC-speaker portion of the INT 08h handler at 1000:3B18. */
uint16_t sr_pc_speaker_effect_tick(
    SrSoundEffectState *state,
    const SrEmbeddedSoundEffects *speaker_effects);

#endif
