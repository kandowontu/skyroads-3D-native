#include "win32_opl_audio.hpp"

#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace skyroads {
namespace {

constexpr std::size_t kBufferFrames = 512;
constexpr std::size_t kBufferSamples = kBufferFrames * 2u;
constexpr std::size_t kBufferCount = 8;
constexpr std::size_t kPrebufferCount = 3;

std::runtime_error wave_error(const char* operation, MMRESULT result) {
    std::array<char, MAXERRORLENGTH> message{};
    if (waveOutGetErrorTextA(result, message.data(),
            static_cast<UINT>(message.size())) != MMSYSERR_NOERROR) {
        std::strcpy(message.data(), "unknown WinMM error");
    }
    return std::runtime_error(
        std::string(operation) + " failed: " + message.data());
}

} // namespace

struct Win32OplAudio::Impl {
    struct Buffer {
        std::array<std::int16_t, kBufferSamples> samples{};
        WAVEHDR header{};
        bool prepared{};
        bool queued{};
    };

    Impl() {
        WAVEFORMATEX format{};
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.nChannels = 2;
        format.nSamplesPerSec = kOplOutputRate;
        format.wBitsPerSample = 16;
        format.nBlockAlign = static_cast<WORD>(
            format.nChannels * format.wBitsPerSample / 8u);
        format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
        const auto opened = waveOutOpen(
            &device, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL);
        if (opened != MMSYSERR_NOERROR) throw wave_error("waveOutOpen", opened);

        try {
            for (auto& buffer : buffers) {
                buffer.header.lpData = reinterpret_cast<LPSTR>(buffer.samples.data());
                buffer.header.dwBufferLength = static_cast<DWORD>(
                    buffer.samples.size() * sizeof(buffer.samples[0]));
                const auto prepared = waveOutPrepareHeader(
                    device, &buffer.header, sizeof(buffer.header));
                if (prepared != MMSYSERR_NOERROR) {
                    throw wave_error("waveOutPrepareHeader", prepared);
                }
                buffer.prepared = true;
            }
        }
        catch (...) {
            close();
            throw;
        }
    }

    ~Impl() { close(); }

    void close() noexcept {
        if (device == nullptr) return;
        waveOutReset(device);
        for (auto& buffer : buffers) {
            if (buffer.prepared) {
                waveOutUnprepareHeader(device, &buffer.header, sizeof(buffer.header));
                buffer.prepared = false;
                buffer.queued = false;
            }
        }
        waveOutClose(device);
        device = nullptr;
    }

    void reap() {
        for (auto& buffer : buffers) {
            if (buffer.queued && (buffer.header.dwFlags & WHDR_DONE) != 0) {
                buffer.queued = false;
            }
        }
    }

    Buffer* free_buffer() {
        for (auto& buffer : buffers) {
            if (!buffer.queued) return &buffer;
        }
        return nullptr;
    }

    void compact_pending() {
        if (pending_offset == 0) return;
        if (pending_offset == pending.size()) {
            pending.clear();
            pending_offset = 0;
        }
        else if (pending_offset >= kBufferSamples * 4u) {
            pending.erase(pending.begin(), pending.begin() +
                static_cast<std::ptrdiff_t>(pending_offset));
            pending_offset = 0;
        }
    }

    void pump() {
        reap();
        const auto available = [&] { return pending.size() - pending_offset; };
        if (!started && available() < kBufferSamples * kPrebufferCount) return;
        started = true;
        while (available() >= kBufferSamples) {
            auto* buffer = free_buffer();
            if (buffer == nullptr) break;
            std::copy_n(pending.data() + pending_offset,
                kBufferSamples, buffer->samples.data());
            pending_offset += kBufferSamples;
            buffer->header.dwFlags &= ~WHDR_DONE;
            const auto submitted = waveOutWrite(
                device, &buffer->header, sizeof(buffer->header));
            if (submitted != MMSYSERR_NOERROR) {
                throw wave_error("waveOutWrite", submitted);
            }
            buffer->queued = true;
        }
        compact_pending();
    }

    HWAVEOUT device{};
    std::array<Buffer, kBufferCount> buffers{};
    OplSynth synth;
    PcmEffectMixer effect_mixer;
    OplIrqFrameClock frame_clock;
    std::vector<std::int16_t> pending;
    std::size_t pending_offset{};
    bool started{};
};

Win32OplAudio::Win32OplAudio() : impl_(std::make_unique<Impl>()) {}
Win32OplAudio::~Win32OplAudio() = default;

void Win32OplAudio::write_registers(
    std::span<const OplRegisterWrite> writes) {
    impl_->synth.write_registers(writes);
}

void Win32OplAudio::start_effect(PcmEffect effect) {
    impl_->effect_mixer.start(std::move(effect));
}

void Win32OplAudio::advance_irq(std::span<const OplRegisterWrite> writes) {
    impl_->synth.write_registers(writes);
    const auto first_sample = impl_->pending.size();
    impl_->synth.render_frames(impl_->frame_clock.advance(), impl_->pending);
    impl_->effect_mixer.mix(std::span<std::int16_t>(impl_->pending).subspan(
        first_sample));
    impl_->pump();
}

} // namespace skyroads
