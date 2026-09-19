#include "opl_synth.hpp"

#include <opal/opal.h>

#include <algorithm>
#include <utility>

namespace skyroads {

std::size_t OplIrqFrameClock::advance() {
    remainder_ += static_cast<std::uint64_t>(kOplOutputRate) * kDosPitDivisor;
    const auto frames = static_cast<std::size_t>(remainder_ / kDosPitClock);
    remainder_ %= kDosPitClock;
    total_frames_ += frames;
    return frames;
}

struct OplSynth::Impl {
    Impl() { opalInit(&chip, static_cast<int>(kOplOutputRate)); }
    Opal chip{};
};

OplSynth::OplSynth() : impl_(std::make_unique<Impl>()) {}
OplSynth::~OplSynth() = default;
OplSynth::OplSynth(OplSynth&&) noexcept = default;
OplSynth& OplSynth::operator=(OplSynth&&) noexcept = default;

void OplSynth::write_registers(std::span<const OplRegisterWrite> writes) {
    for (const auto& write : writes) {
        opalWriteRegBuffered(&impl_->chip, write.reg, write.value);
    }
}

void OplSynth::render_frames(
    std::size_t frame_count,
    std::vector<std::int16_t>& output) {
    const auto first_sample = output.size();
    output.resize(first_sample + frame_count * 2u);
    for (std::size_t frame = 0; frame < frame_count; ++frame) {
        std::int16_t left{};
        std::int16_t right{};
        opalSample(&impl_->chip, &left, &right);
        (void)right;
        /* SkyRoads addresses the original mono OPL2 register bank only. */
        output[first_sample + frame * 2u] = left;
        output[first_sample + frame * 2u + 1u] = left;
    }
}

void PcmEffectMixer::start(PcmEffect effect) {
    effect_ = std::move(effect);
    source_phase_ = 0;
    if (effect_.sample_rate == 0 || effect_.samples.empty()) {
        effect_ = {};
    }
}

void PcmEffectMixer::mix(
    std::span<std::int16_t> interleaved_stereo) {
    if (!active()) return;
    const auto frame_count = interleaved_stereo.size() / 2u;
    for (std::size_t frame = 0; frame < frame_count; ++frame) {
        const auto source_index = static_cast<std::size_t>(
            source_phase_ / kOplOutputRate);
        if (source_index >= effect_.samples.size()) {
            if (!effect_.loop) {
                effect_ = {};
                source_phase_ = 0;
                break;
            }
            source_phase_ %= static_cast<std::uint64_t>(
                effect_.samples.size()) * kOplOutputRate;
        }
        const auto wrapped_source_index = static_cast<std::size_t>(
            source_phase_ / kOplOutputRate);
        const auto next_index = effect_.loop
            ? (wrapped_source_index + 1u) % effect_.samples.size()
            : std::min(wrapped_source_index + 1u, effect_.samples.size() - 1u);
        const auto fraction = source_phase_ % kOplOutputRate;
        const auto first = static_cast<std::int64_t>(
            static_cast<int>(effect_.samples[wrapped_source_index]) - 128);
        const auto second = static_cast<std::int64_t>(
            static_cast<int>(effect_.samples[next_index]) - 128);
        const auto interpolated =
            (first * static_cast<std::int64_t>(kOplOutputRate - fraction) +
             second * static_cast<std::int64_t>(fraction)) /
            static_cast<std::int64_t>(kOplOutputRate);
        const auto sample = static_cast<int>(interpolated * 256);
        for (std::size_t channel = 0; channel < 2u; ++channel) {
            const auto index = frame * 2u + channel;
            const auto mixed = static_cast<int>(interleaved_stereo[index]) + sample;
            interleaved_stereo[index] = static_cast<std::int16_t>(
                std::clamp(mixed, -32768, 32767));
        }
        source_phase_ += effect_.sample_rate;
    }
}

} // namespace skyroads
