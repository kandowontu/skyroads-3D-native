#include "sound_effects.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct HookState {
    unsigned speaker_calls;
    unsigned sample_calls;
    size_t sample_bytes;
    uint8_t time_constant;
} HookState;

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

static void enable_speaker(void *context) {
    ++((HookState *)context)->speaker_calls;
}

static void start_sample(
    void *context,
    const uint8_t *bytes,
    size_t byte_count,
    uint8_t time_constant) {
    HookState *state = (HookState *)context;
    (void)bytes;
    ++state->sample_calls;
    state->sample_bytes = byte_count;
    state->time_constant = time_constant;
}

int main(int argc, char **argv) {
    uint8_t *exe_bytes;
    uint8_t *sfx_bytes;
    size_t exe_size;
    size_t sfx_size;
    SrEmbeddedSoundEffects embedded;
    SrSampleEffectArchive samples;
    SrSoundEffectState state;
    SrSoundEffectHooks hooks;
    HookState calls = {0};
    SrSampleEffect sample;
    if (argc != 3) return 1;
    exe_bytes = read_file(argv[1], &exe_size);
    sfx_bytes = read_file(argv[2], &sfx_size);
    if (exe_bytes == 0 || sfx_bytes == 0 ||
        !sr_load_embedded_sound_effects_from_exe(exe_bytes, exe_size, &embedded) ||
        !sr_load_sample_effect_archive(sfx_bytes, sfx_size, &samples)) return 1;
    free(exe_bytes); free(sfx_bytes);
    if (embedded.note_counts[0] != 13 || embedded.notes[0][0] != 0x3a98u ||
        embedded.note_counts[4] != 6 || embedded.notes[4][4] != 0x012cu ||
        !sr_get_sample_effect(&samples, 4, &sample) ||
        sample.dsp_time_constant != 131 || sample.byte_count != 7770) return 1;

    hooks.context = &calls;
    hooks.enable_pc_speaker = enable_speaker;
    hooks.start_sample = start_sample;
    sr_sound_effect_state_init(&state, 0);
    sr_sound_effect_set_tick(&state, 100);
    if (!sr_play_sound_effect(&state, 2, &embedded, &samples, &hooks) ||
        calls.speaker_calls != 1 || !sr_sound_effect_is_active(&state, &embedded) ||
        sr_pc_speaker_effect_tick(&state, &embedded) != 0x3a9au ||
        sr_sound_effect_is_active(&state, &embedded)) return 1;

    sr_sound_effect_state_init(&state, 1);
    sr_sound_effect_set_tick(&state, 200);
    if (!sr_play_sound_effect(&state, 1, &embedded, &samples, &hooks) ||
        calls.sample_calls != 1 || calls.sample_bytes != 5153 ||
        calls.time_constant != 131 || !sr_sound_effect_is_active(&state, &embedded)) {
        return 1;
    }
    sr_sound_effect_set_tick(&state, 208);
    if (sr_sound_effect_is_active(&state, &embedded)) return 1;
    sr_free_sample_effect_archive(&samples);
    return 0;
}
