#pragma once

#include "recovered_game.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace skyroads {

inline constexpr std::uint32_t kOplOutputRate = 49716u;

class OplIrqFrameClock final {
public:
    [[nodiscard]] std::size_t advance();
    [[nodiscard]] std::uint64_t total_frames() const { return total_frames_; }

private:
    std::uint64_t remainder_{};
    std::uint64_t total_frames_{};
};

class OplSynth final {
public:
    OplSynth();
    ~OplSynth();
    OplSynth(OplSynth&&) noexcept;
    OplSynth& operator=(OplSynth&&) noexcept;
    OplSynth(const OplSynth&) = delete;
    OplSynth& operator=(const OplSynth&) = delete;

    void write_registers(std::span<const OplRegisterWrite> writes);
    void render_frames(std::size_t frame_count, std::vector<std::int16_t>& output);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/* The DOS Sound Blaster path played unsigned 8-bit samples alongside the
   AdLib music.  Mix that second hardware stream into the native stereo
   output without disturbing the executable-recovered OPL state. */
class PcmEffectMixer final {
public:
    void start(PcmEffect effect);
    void mix(std::span<std::int16_t> interleaved_stereo);
    [[nodiscard]] bool active() const { return !effect_.samples.empty(); }

private:
    PcmEffect effect_{};
    std::uint64_t source_phase_{};
};

} // namespace skyroads
