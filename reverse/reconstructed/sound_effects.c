#include "sound_effects.h"

#include <stdlib.h>
#include <string.h>

static uint16_t read_u16(const uint8_t *bytes) {
    return (uint16_t)(bytes[0] | ((uint16_t)bytes[1] << 8));
}

int sr_load_embedded_sound_effects_from_exe(
    const uint8_t *bytes,
    size_t size,
    SrEmbeddedSoundEffects *effects) {
    size_t header_size;
    size_t data_segment;
    unsigned effect;
    if (bytes == 0 || effects == 0 || size < 0x1cu ||
        bytes[0] != 'M' || bytes[1] != 'Z') return 0;
    header_size = (size_t)read_u16(bytes + 8u) * 16u;
    data_segment = header_size + 0x066eu * 16u;
    if (data_segment > size || 0x013cu > size - data_segment) return 0;
    memset(effects, 0, sizeof(*effects));
    for (effect = 0; effect < SR_SOUND_EFFECT_COUNT; ++effect) {
        uint16_t cursor = read_u16(
            bytes + data_segment + 0x0132u + effect * 2u);
        size_t count = 0;
        while (count < SR_PC_SPEAKER_MAX_NOTES) {
            uint16_t note;
            if (cursor > size - data_segment ||
                2u > size - data_segment - cursor) return 0;
            note = read_u16(bytes + data_segment + cursor);
            if (note == 0) break;
            effects->notes[effect][count++] = note;
            cursor = (uint16_t)(cursor + 2u);
        }
        if (count == SR_PC_SPEAKER_MAX_NOTES) return 0;
        effects->note_counts[effect] = count;
    }
    return 1;
}

void sr_free_sample_effect_archive(SrSampleEffectArchive *archive) {
    if (archive == 0) return;
    free(archive->bytes);
    memset(archive, 0, sizeof(*archive));
}

int sr_load_sample_effect_archive(
    const uint8_t *bytes,
    size_t size,
    SrSampleEffectArchive *archive) {
    unsigned index;
    if (bytes == 0 || archive == 0 || size < 12u || size > UINT16_MAX) return 0;
    memset(archive, 0, sizeof(*archive));
    for (index = 0; index <= SR_SOUND_EFFECT_COUNT; ++index) {
        uint16_t offset = read_u16(bytes + index * 2u);
        if (offset > size ||
            (index < SR_SOUND_EFFECT_COUNT && offset == size) ||
            (index != 0 && offset <= archive->offsets[index - 1])) {
            return 0;
        }
        archive->offsets[index] = offset;
    }
    archive->bytes = (uint8_t *)malloc(size);
    if (archive->bytes == 0) return 0;
    memcpy(archive->bytes, bytes, size);
    archive->size = size;
    return 1;
}

int sr_get_sample_effect(
    const SrSampleEffectArchive *archive,
    unsigned effect,
    SrSampleEffect *sample) {
    size_t start;
    size_t end;
    if (archive == 0 || sample == 0 || archive->bytes == 0 ||
        effect >= SR_SOUND_EFFECT_COUNT) return 0;
    start = archive->offsets[effect];
    end = archive->offsets[effect + 1u];
    if (start >= end || end > archive->size) return 0;
    sample->dsp_time_constant = archive->bytes[start];
    sample->bytes = archive->bytes + start + 1u;
    sample->byte_count = end - start - 1u;
    return 1;
}

void sr_sound_effect_state_init(
    SrSoundEffectState *state,
    int sampled_backend) {
    if (state == 0) return;
    memset(state, 0, sizeof(*state));
    state->sampled_backend = sampled_backend != 0;
    state->speaker_effect = 0xffu;
}

void sr_sound_effect_set_tick(SrSoundEffectState *state, uint16_t tick_count) {
    if (state != 0) state->tick_count = tick_count;
}

int sr_play_sound_effect(
    SrSoundEffectState *state,
    unsigned effect,
    const SrEmbeddedSoundEffects *speaker_effects,
    const SrSampleEffectArchive *sample_effects,
    const SrSoundEffectHooks *hooks) {
    if (state == 0 || effect >= SR_SOUND_EFFECT_COUNT) return 0;
    /* 03C8 records the tick even when sound is disabled. */
    state->last_effect_tick = state->tick_count;
    if (state->muted) return 1;
    if (state->sampled_backend) {
        SrSampleEffect sample;
        if (!sr_get_sample_effect(sample_effects, effect, &sample)) return 0;
        if (hooks != 0 && hooks->start_sample != 0) {
            hooks->start_sample(hooks->context, sample.bytes,
                sample.byte_count, sample.dsp_time_constant);
        }
    }
    else {
        if (speaker_effects == 0) return 0;
        if (hooks != 0 && hooks->enable_pc_speaker != 0) {
            hooks->enable_pc_speaker(hooks->context);
        }
        state->speaker_effect = (uint8_t)effect;
        state->speaker_note_index = 0;
    }
    return 1;
}

int sr_sound_effect_is_active(
    const SrSoundEffectState *state,
    const SrEmbeddedSoundEffects *speaker_effects) {
    if (state == 0) return 0;
    if (state->sampled_backend) {
        return state->tick_count < (uint16_t)(state->last_effect_tick + 8u);
    }
    if (speaker_effects == 0 || state->speaker_effect >= SR_SOUND_EFFECT_COUNT) {
        return 0;
    }
    return state->speaker_note_index <
        speaker_effects->note_counts[state->speaker_effect];
}

uint16_t sr_pc_speaker_effect_tick(
    SrSoundEffectState *state,
    const SrEmbeddedSoundEffects *speaker_effects) {
    uint16_t note = 0;
    if (state == 0) return 2;
    state->tick_count = (uint16_t)(state->tick_count + 1u);
    if (speaker_effects != 0 && state->speaker_effect < SR_SOUND_EFFECT_COUNT &&
        state->speaker_note_index <
            speaker_effects->note_counts[state->speaker_effect]) {
        note = speaker_effects->notes[state->speaker_effect]
            [state->speaker_note_index++];
    }
    return (uint16_t)(note + 2u);
}
