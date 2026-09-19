#pragma once
#include "recovered_game.hpp"
#include <array>

namespace skyroads {
// XInput button values; kept platform-independent for deterministic tests.
namespace pad_button {
constexpr unsigned up=0x0001, down=0x0002, left=0x0004, right=0x0008;
constexpr unsigned start=0x0010, back=0x0020, left_stick=0x0040;
constexpr unsigned lb=0x0100, rb=0x0200;
constexpr unsigned a=0x1000, b=0x2000, x=0x4000, y=0x8000;
}
struct ControllerSample {
    bool connected{};
    unsigned buttons{};
    int left_x{}, left_y{};
};
class ControllerInput {
public:
    void merge(const ControllerSample& sample, NativeScreen screen,
        std::uint64_t milliseconds, NativeInput& input);
    void reset();
private:
    unsigned previous_buttons_{};
    std::array<bool,4> directions_{};
    std::array<std::uint64_t,4> repeat_at_{};
};
}
