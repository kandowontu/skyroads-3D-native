#pragma once

#include "opl_synth.hpp"

#include <memory>
#include <span>

namespace skyroads {

class Win32OplAudio final {
public:
    Win32OplAudio();
    ~Win32OplAudio();
    Win32OplAudio(const Win32OplAudio&) = delete;
    Win32OplAudio& operator=(const Win32OplAudio&) = delete;

    void write_registers(std::span<const OplRegisterWrite> writes);
    void start_effect(PcmEffect effect);
    void advance_irq(std::span<const OplRegisterWrite> writes);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace skyroads
