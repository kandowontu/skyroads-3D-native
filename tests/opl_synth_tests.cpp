#include "opl_synth.hpp"
#include "recovered_game.hpp"

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::uint64_t hash_pcm(const std::vector<std::int16_t>& samples) {
    std::uint64_t hash = UINT64_C(1469598103934665603);
    for (const auto sample : samples) {
        const auto value = static_cast<std::uint16_t>(sample);
        hash ^= static_cast<std::uint8_t>(value);
        hash *= UINT64_C(1099511628211);
        hash ^= static_cast<std::uint8_t>(value >> 8u);
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const auto root = argc > 1
            ? std::filesystem::path(argv[1])
            : std::filesystem::current_path();
        skyroads::RecoveredGame game(root);
        skyroads::OplSynth synth;
        skyroads::OplIrqFrameClock frame_clock;
        std::vector<std::int16_t> pcm;

        auto writes = game.consume_opl_writes();
        require(writes.size() == 93u,
            "Native OPL bridge lost startup or intro-track register writes");
        require(writes.front().reg == 0x40u && writes.front().value == 0x3fu,
            "Native OPL bridge changed the recovered initialization order");
        synth.write_registers(writes);

        skyroads::NativeInput input;
        for (unsigned irq = 0; irq < 900u; ++irq) {
            game.timer_tick(input);
            writes = game.consume_opl_writes();
            synth.write_registers(writes);
            synth.render_frames(frame_clock.advance(), pcm);
        }
        const auto expected_frames =
            static_cast<std::uint64_t>(900u) * skyroads::kOplOutputRate *
            skyroads::kDosPitDivisor / skyroads::kDosPitClock;
        require(pcm.size() == expected_frames * 2u,
            "OPL PCM clock drifted from the original PIT divisor");
        bool nonzero = false;
        for (std::size_t frame = 0; frame < pcm.size() / 2u; ++frame) {
            require(pcm[frame * 2u] == pcm[frame * 2u + 1u],
                "Original OPL2 output must remain mono");
            nonzero = nonzero || pcm[frame * 2u] != 0;
        }
        require(nonzero, "Native OPL2 synthesis produced silence");
        const auto pcm_hash = hash_pcm(pcm);
        require(pcm_hash == UINT64_C(0x62c3201a0e1ada87),
            "Native OPL2 PCM changed for the executable-verified register stream");

        skyroads::PcmEffectMixer effect_mixer;
        skyroads::PcmEffect effect;
        effect.sample_rate = skyroads::kOplOutputRate;
        effect.samples = {128u, 255u, 0u};
        std::vector<std::int16_t> mixed(10u, 1000);
        effect_mixer.start(std::move(effect));
        effect_mixer.mix(mixed);
        require(mixed[0] == 1000 && mixed[1] == 1000,
            "Centered Sound Blaster PCM changed the OPL sample");
        require(mixed[2] == 32767 && mixed[3] == 32767,
            "Positive Sound Blaster PCM did not saturate cleanly");
        require(mixed[4] == -31768 && mixed[5] == -31768,
            "Negative Sound Blaster PCM did not mix cleanly");
        require(mixed[6] == 1000 && mixed[7] == 1000 &&
                mixed[8] == 1000 && mixed[9] == 1000 && !effect_mixer.active(),
            "Completed Sound Blaster PCM disturbed later music samples");
        std::cout << "OPL PCM hash " << std::hex << pcm_hash
                  << " over " << std::dec << expected_frames << " frames\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
