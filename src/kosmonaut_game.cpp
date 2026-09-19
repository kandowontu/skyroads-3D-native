#include "kosmonaut_game.hpp"

#include "kosmonaut_assets.hpp"
#include "recovered_game.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <string>

namespace skyroads {
namespace {

constexpr unsigned kWidth = kKosmonautScreenWidth;
constexpr unsigned kHeight = kKosmonautScreenHeight;
constexpr unsigned kScreenBytes = kWidth * kHeight;
constexpr unsigned kRoadBytes = kKosmonautRoadRows * kKosmonautRoadColumns;

std::uint8_t ega_component(std::uint8_t reg, unsigned low_bit, unsigned high_bit) {
    return static_cast<std::uint8_t>(
        (((reg >> low_bit) & 1u) ? 0x2au : 0u) +
        (((reg >> high_bit) & 1u) ? 0x15u : 0u));
}

void put_pixel(
    std::vector<std::uint8_t>& pixels, int x, int y, std::uint8_t color) {
    if (x < 0 || x >= static_cast<int>(kWidth) ||
        y < 0 || y >= static_cast<int>(kHeight)) return;
    pixels[static_cast<std::size_t>(y) * kWidth + static_cast<unsigned>(x)] = color;
}

void fill_rect(
    std::vector<std::uint8_t>& pixels,
    int left, int top, int right, int bottom, std::uint8_t color) {
    left = std::clamp(left, 0, static_cast<int>(kWidth));
    right = std::clamp(right, 0, static_cast<int>(kWidth));
    top = std::clamp(top, 0, static_cast<int>(kHeight));
    bottom = std::clamp(bottom, 0, static_cast<int>(kHeight));
    if (left >= right || top >= bottom) return;
    for (int y = top; y < bottom; ++y) {
        std::fill(
            pixels.begin() + static_cast<std::ptrdiff_t>(y * kWidth + left),
            pixels.begin() + static_cast<std::ptrdiff_t>(y * kWidth + right),
            color);
    }
}

void draw_line(
    std::vector<std::uint8_t>& pixels,
    int x0, int y0, int x1, int y1, std::uint8_t color) {
    const int dx = std::abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    while (true) {
        put_pixel(pixels, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        const int twice = error * 2;
        if (twice >= dy) {
            error += dy;
            x0 += sx;
        }
        if (twice <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}

int glyph_index(char value) {
    if (value >= 'A' && value <= 'Z') return value - 0x34;
    if (value >= 'a' && value <= 'z') return value - 0x3a;
    if (value >= '0' && value <= '9') return value + 0x11;
    switch (value) {
    case '\x18': return 0x50;
    case '\x19': return 0x51;
    case '!': return 0x4d;
    case ',': return 0x4c;
    case '.': return 0x4b;
    case '*': return 0x4e;
    case '&': return 0x4f;
    case '%': return 0x52;
    case '\'': return 0x53;
    case '?': return 0x54;
    case '-': return 0x55;
    case '#': return 0x56;
    default: return -1;
    }
}

void draw_glyph(
    std::vector<std::uint8_t>& pixels, int x, int y,
    int glyph, unsigned style, unsigned scale = 1u) {
    const auto font = kosmonaut_font();
    const auto styles = kosmonaut_font_styles();
    if (glyph < 0 || static_cast<std::size_t>(glyph * 8 + 7) >= font.size()) return;
    style = std::min<unsigned>(style, 4u);
    for (int row = 0; row < 8; ++row) {
        const auto bits = font[static_cast<std::size_t>(glyph * 8 + row)];
        const auto color = styles[style * 8u + static_cast<unsigned>(row)];
        for (int column = 0; column < 8; ++column) {
            if ((bits & (0x80u >> column)) == 0u) continue;
            fill_rect(pixels,
                x + column * static_cast<int>(scale),
                y + row * static_cast<int>(scale),
                x + (column + 1) * static_cast<int>(scale),
                y + (row + 1) * static_cast<int>(scale), color);
        }
    }
}

void draw_char(
    std::vector<std::uint8_t>& pixels, int x, int y,
    char value, unsigned style, unsigned scale = 1u) {
    draw_glyph(pixels, x, y, glyph_index(value), style, scale);
}

void draw_text(
    std::vector<std::uint8_t>& pixels, int x, int y,
    const std::string& text, unsigned style, unsigned scale = 1u) {
    for (const char value : text) {
        draw_char(pixels, x, y, value, style, scale);
        x += static_cast<int>(8u * scale);
    }
}

void draw_centered(
    std::vector<std::uint8_t>& pixels, int y,
    const std::string& text, unsigned style, unsigned scale = 1u) {
    const int width = static_cast<int>(text.size() * 8u * scale);
    draw_text(pixels, (static_cast<int>(kWidth) - width) / 2, y, text, style, scale);
}

std::string padded_number(unsigned value, unsigned digits) {
    auto result = std::to_string(value);
    if (result.size() < digits) result.insert(result.begin(), digits - result.size(), '0');
    return result;
}

std::uint16_t table_word(
    std::span<const std::uint8_t> table, std::size_t offset) {
    if (offset + 1u >= table.size()) return 0u;
    return static_cast<std::uint16_t>(
        table[offset] | (static_cast<unsigned>(table[offset + 1u]) << 8u));
}

void draw_scroll(
    std::vector<std::uint8_t>& pixels,
    std::span<const std::uint8_t> text, unsigned tick,
    unsigned cadence, unsigned style) {
    const unsigned step_tick = cadence == 0u ? tick : tick / cadence;
    const unsigned group_count = std::max<unsigned>(
        1u, (static_cast<unsigned>(text.size()) + 42u) / 3u);
    int character = -40 + static_cast<int>(
        ((step_tick / 8u) % group_count) * 3u);
    int x = -static_cast<int>((step_tick % 8u) * 3u);
    const int reset_at = static_cast<int>(text.size());
    for (; x < static_cast<int>(kWidth); x += 8, ++character) {
        if (character >= 0 && character < reset_at) {
            draw_char(pixels, x, 0xc0,
                static_cast<char>(text[static_cast<std::size_t>(character)]),
                style);
        }
    }
}

} // namespace

KosmonautGame::KosmonautGame(std::filesystem::path save_directory)
    : save_path_(std::move(save_directory) / "HISCORES.SKY"),
      pixels_(kScreenBytes) {
    const auto registers = kosmonaut_palette_registers();
    for (unsigned index = 0; index < 16u && index < registers.size(); ++index) {
        const auto reg = registers[index];
        palette_[index * 3u + 0u] = ega_component(reg, 2u, 5u);
        palette_[index * 3u + 1u] = ega_component(reg, 1u, 4u);
        palette_[index * 3u + 2u] = ega_component(reg, 0u, 3u);
    }
    reset_starfield();
    load_save();
    render();
}

void KosmonautGame::load_save() {
    std::ifstream stream(save_path_, std::ios::binary);
    std::array<std::uint8_t, 0x110> data{};
    if (!stream.read(reinterpret_cast<char*>(data.data()), data.size())) return;
    for (auto& value : data) value ^= 0xa5u;
    unlocked_roads_ = std::clamp<unsigned>(
        static_cast<unsigned>(data[0] | (data[1] << 8u)), 2u,
        kKosmonautRoadCount);
    best_score_ = 0u;
    for (unsigned index = 0; index < high_scores_.size(); ++index) {
        const auto offset = 2u + index * 0x1eu;
        auto& entry = high_scores_[index];
        entry.name.assign(
            reinterpret_cast<const char*>(data.data() + offset), 25u);
        while (!entry.name.empty() &&
            (entry.name.back() == ' ' || entry.name.back() == '\0')) {
            entry.name.pop_back();
        }
        entry.score = static_cast<std::uint16_t>(
            data[offset + 0x1au] | (data[offset + 0x1bu] << 8u));
        entry.road = data[offset + 0x1cu];
        best_score_ = std::max<std::uint32_t>(best_score_, entry.score);
    }
}

void KosmonautGame::save_progress() const {
    std::ofstream stream(save_path_, std::ios::binary | std::ios::trunc);
    if (!stream) return;
    std::array<std::uint8_t, 0x110> data{};
    data[0] = static_cast<std::uint8_t>(unlocked_roads_ & 0xffu);
    data[1] = static_cast<std::uint8_t>((unlocked_roads_ >> 8u) & 0xffu);
    for (unsigned index = 0; index < high_scores_.size(); ++index) {
        const auto offset = 2u + index * 0x1eu;
        const auto& entry = high_scores_[index];
        std::fill_n(data.begin() + offset, 25u, static_cast<std::uint8_t>(' '));
        for (unsigned character = 0;
            character < entry.name.size() && character < 25u; ++character) {
            data[offset + character] =
                static_cast<std::uint8_t>(entry.name[character]);
        }
        data[offset + 0x1au] = static_cast<std::uint8_t>(entry.score & 0xffu);
        data[offset + 0x1bu] = static_cast<std::uint8_t>(entry.score >> 8u);
        data[offset + 0x1cu] = entry.road;
    }
    for (auto& value : data) value ^= 0xa5u;
    stream.write(reinterpret_cast<const char*>(data.data()), data.size());
}

void KosmonautGame::enter() {
    phase_ = Phase::Title;
    phase_ticks_ = 0u;
    reset_starfield();
    silence_sound();
    render();
}

void KosmonautGame::reset_road(bool demo) {
    forward_position_ = checkpoint_reached_ && !demo ? 400 : 4;
    forward_fraction_ = 0;
    forward_speed_ = 0;
    lateral_position_ = 0xc0;
    lateral_velocity_ = 0;
    altitude_ = 9;
    vertical_velocity_ = 0;
    fuel_ = 32000;
    oxygen_ = 32000;
    jump_power_ = checkpoint_reached_ && !demo ? saved_jump_power_ : 0x78;
    previous_forward_position_ = forward_position_;
    previous_lateral_position_ = lateral_position_;
    previous_altitude_ = altitude_;
    lateral_step_ = 0;
    speed_penalty_ = 0;
    bounce_height_ = 0;
    bounce_velocity_ = 0;
    grounded_ = true;
    slippery_ = false;
    speed_penalty_active_ = false;
    dying_ = false;
    death_ticks_ = 0u;
    burning_ = false;
    demo_session_ = demo;
    if (demo) session_had_input_ = false;
    phase_ticks_ = 0;
    ship_animation_ = 0u;
    last_effect_tile_ = 0;
    phase_ = demo ? Phase::Demo : Phase::Playing;
    sound_tone(0, 660, 50);
    render();
}

unsigned KosmonautGame::current_row() const {
    return std::min<unsigned>(kKosmonautRoadRows - 1u,
        static_cast<unsigned>(std::max(0, forward_position_) >> 2));
}

std::uint8_t KosmonautGame::current_tile() const {
    const int lane = (lateral_position_ - 0x20) / 0x32;
    if (lane < 0 || lane >= static_cast<int>(kKosmonautRoadColumns)) return 0u;
    return road_tile(current_row(), static_cast<unsigned>(lane));
}

std::uint8_t KosmonautGame::road_tile(unsigned row, unsigned lane) const {
    const bool tutorial = phase_ == Phase::Tutorial;
    const auto roads = tutorial ? kosmonaut_tutorial_road() :
        (demo_session_ ? kosmonaut_demo_road() : kosmonaut_roads());
    const auto offset = tutorial || demo_session_
        ? static_cast<std::size_t>(row) * kKosmonautRoadColumns + lane
        : static_cast<std::size_t>(selected_road_) * kRoadBytes +
            row * kKosmonautRoadColumns + lane;
    if (offset >= roads.size()) return 0u;
    auto tile = roads[offset];

    if (tutorial) return tile;
    const auto effects = demo_session_
        ? kosmonaut_demo_effects() : kosmonaut_level_effects();
    const std::size_t record = demo_session_ ? 0u :
        static_cast<std::size_t>(selected_road_) * 0x32u;
    if (record >= effects.size()) return tile;
    const unsigned count = std::min<unsigned>(effects[record], 12u);
    const unsigned local_offset = row * kKosmonautRoadColumns + lane;
    for (unsigned entry = 0; entry < count; ++entry) {
        const std::size_t item = record + 2u + entry * 4u;
        if (item + 3u >= effects.size()) break;
        const unsigned target = effects[item] |
            (static_cast<unsigned>(effects[item + 1u]) << 8u);
        if (target != local_offset) continue;
        const auto alternate = static_cast<std::uint8_t>(effects[item + 2u]);
        tile = ((animation_tick_ / 7u) & 1u) == 0u ? alternate : 0u;
        break;
    }
    return tile;
}

int KosmonautGame::apply_tile_effect(std::uint8_t tile) {
    const std::uint8_t effect = tile & 0x0fu;
    const bool newly_entered = effect != last_effect_tile_;
    last_effect_tile_ = effect;
    if (altitude_ > 9) tile = 10u;
    else if (altitude_ < 9) tile = 0u;
    const auto live_effect = static_cast<std::uint8_t>(tile & 0x0fu);
    burning_ = live_effect == 4u;
    if (live_effect == 0u) {
        return dying_ || burning_ || slippery_ ? 0 : 1;
    }
    switch (live_effect) {
    case 1u:
        if (!dying_) {
            if (newly_entered && 32000 - fuel_ > 3200) sound_tone(1, 980, 45);
            fuel_ = 32000;
        }
        break;
    case 2u:
        if (forward_speed_ > 4) forward_speed_ -= 4;
        break;
    case 3u:
        if (!dying_) {
            if (newly_entered && 32000 - oxygen_ > 3200) sound_tone(3, 860, 45);
            oxygen_ = 32000;
        }
        break;
    case 4u:
        if (newly_entered) sound_tone(4, 600, 120);
        break;
    case 5u:
        if (jump_power_ < 119 && !dying_) {
            jump_power_ += 7;
            if (newly_entered) sound_tone(5, 720, 35);
        }
        break;
    case 6u:
        if (jump_power_ > 6 && !dying_) {
            jump_power_ -= 7;
            if (newly_entered) sound_tone(6, 380, 35);
        }
        break;
    case 7u:
        slippery_ = true;
        return 2;
    case 8u:
        if (!dying_) {
            if (newly_entered &&
                (32000 - oxygen_ > 3200 || jump_power_ < 100 ||
                 32000 - fuel_ > 3200)) {
                sound_tone(8, 1100, 65);
            }
            jump_power_ = std::min(120, jump_power_ + 20);
            fuel_ = 32000;
            oxygen_ = 32000;
        }
        break;
    case 9u:
        forward_speed_ += 4;
        break;
    default:
        break;
    }
    if (lateral_position_ < 0x2a || lateral_position_ > 0x156) return 1;
    if (!dying_) {
        const int effective_speed = speed_penalty_active_
            ? forward_speed_ - speed_penalty_ : forward_speed_;
        fuel_ -= (effective_speed >> 6) * 10;
        oxygen_ -= 0x28;
    }
    if (fuel_ < 1) {
        fuel_ = 0;
        return 1;
    }
    if (oxygen_ < 1) {
        oxygen_ = 0;
        return 1;
    }
    return 0;
}

bool KosmonautGame::test_collision(
    std::int32_t forward, std::int32_t lateral,
    std::int32_t altitude, std::int32_t old_altitude) {
    const auto sample = [this, forward](int lateral_offset,
                           int lower, int upper) -> std::uint8_t {
        const int lateral = lateral_offset;
        if (lateral < lower || lateral >= upper) return 0u;
        const int lane = (lateral - lower) / 0x32;
        const int row = (forward - 1) >> 2;
        if (row < 0 || row >= static_cast<int>(kKosmonautRoadRows) ||
            lane < 0 || lane >= static_cast<int>(kKosmonautRoadColumns)) {
            return 0u;
        }
        return road_tile(static_cast<unsigned>(row), static_cast<unsigned>(lane));
    };
    surface_tile_ = sample(lateral, 0x2a, 0x156);
    left_tile_ = sample(lateral, 0x34, 0x160);
    right_tile_ = sample(lateral, 0x20, 0x14c);

    const bool has_surface = (surface_tile_ % 0x10u) != 0u ||
        (left_tile_ % 0x10u) != 0u || (right_tile_ % 0x10u) != 0u;
    if ((left_tile_ == 0x0fu || right_tile_ == 0x0fu) &&
        altitude >= 9 && altitude <= 24) {
        return true;
    }

    const int remainder = (lateral - 0x2a) % 0x32;
    const int distance = std::abs(0x19 - remainder);
    bool crossed_profile = false;
    if (altitude >= 9 && altitude < 24) {
        const auto tables = kosmonaut_simulation_tables();
        const auto profile = static_cast<int>(
            table_word(tables, 0x88u + static_cast<unsigned>(altitude - 9) * 2u));
        crossed_profile = profile < distance;
    }
    if (altitude <= 24 && old_altitude > 24) crossed_profile = true;
    if (altitude > 24 && old_altitude <= 24) crossed_profile = true;
    if (left_tile_ >= 0x10u && right_tile_ >= 0x10u && crossed_profile) {
        return true;
    }
    if (has_surface && altitude < 9 && altitude > -11) return true;
    return lateral < 0;
}

bool KosmonautGame::predicts_safe_landing(
    std::int32_t speed, std::int32_t lateral_step) const {
    int local_altitude = altitude_;
    int local_vertical = vertical_velocity_;
    int local_forward = forward_position_;
    int local_fraction = forward_fraction_;
    int local_lateral = lateral_position_;
    std::uint8_t tile = 0u;
    for (;;) {
        if (local_lateral >= 0x2a && local_lateral < 0x156) {
            const int lane = (local_lateral - 0x2a) / 0x32;
            const int row = (local_forward - 1) >> 2;
            if (row >= 0 && row < static_cast<int>(kKosmonautRoadRows) &&
                lane >= 0 && lane < static_cast<int>(kKosmonautRoadColumns)) {
                tile = road_tile(static_cast<unsigned>(row),
                    static_cast<unsigned>(lane));
            }
            else tile = 0u;
        }
        else tile = 0u;
        local_altitude += local_vertical >> 3;
        if (local_altitude < 10) break;
        local_vertical -= 0x20;
        local_lateral += lateral_step;
        local_fraction += speed;
        if (local_fraction > 0xfe) {
            local_forward += local_fraction / 0xff;
            local_fraction %= 0xff;
        }
    }
    return tile != 0u && tile != 6u && tile != 7u && tile != 0x0fu;
}

void KosmonautGame::find_landing_adjustment() {
    const int original_speed = forward_speed_;
    const int original_lateral = lateral_step_;
    for (forward_speed_ = original_speed + 5;
         forward_speed_ < original_speed + 0x3c; forward_speed_ += 5) {
        if (predicts_safe_landing(forward_speed_, lateral_step_)) {
            speed_penalty_ = forward_speed_ - original_speed;
            return;
        }
    }
    forward_speed_ = original_speed;
    for (forward_speed_ = original_speed - 5;
         forward_speed_ > original_speed - 0x3c; forward_speed_ -= 5) {
        if (predicts_safe_landing(forward_speed_, lateral_step_)) {
            if (forward_speed_ < 0) forward_speed_ = original_speed;
            speed_penalty_ = forward_speed_ - original_speed;
            return;
        }
    }
    forward_speed_ = original_speed;
    if (original_lateral == 0) return;
    for (lateral_step_ = original_lateral + 1;
         lateral_step_ < original_lateral + 10; ++lateral_step_) {
        if (predicts_safe_landing(forward_speed_, lateral_step_)) return;
    }
    lateral_step_ = original_lateral;
    for (lateral_step_ = original_lateral - 1;
         lateral_step_ > original_lateral - 10; --lateral_step_) {
        if (predicts_safe_landing(forward_speed_, lateral_step_)) return;
    }
    lateral_step_ = original_lateral;
}

void KosmonautGame::update_playing(const NativeInput& input, bool demo) {
    ++animation_tick_;
    ++phase_ticks_;
    ship_animation_ = (ship_animation_ + 1u) % 5u;

    bool left = input.left;
    bool right = input.right;
    bool up = input.up;
    bool down = input.down;
    bool jump = input.jump;
    if (demo) {
        const auto record = kosmonaut_demo_input();
        const auto command = record.empty() ? 0xffu :
            record[std::min<std::size_t>(
                static_cast<std::size_t>(std::max(0, forward_position_)),
                record.size() - 1u)];
        left = command == 0u || command == 5u;
        right = command == 1u || command == 6u;
        up = command == 2u;
        down = command == 3u;
        jump = command == 4u || command == 5u || command == 6u;
    }

    if (!demo) {
        if (left || right || up || down || jump) {
            idle_ticks_ = 0u;
            session_had_input_ = true;
        }
        else ++idle_ticks_;
        if (idle_ticks_ >= 0x316u) {
            finish_session();
            render();
            return;
        }
    }

    if (test_collision(
            forward_position_, lateral_position_, altitude_, previous_altitude_)) {
        forward_speed_ -= speed_penalty_;
        speed_penalty_active_ = false;
        speed_penalty_ = 0;
        if (test_collision(forward_position_, previous_lateral_position_,
                altitude_, previous_altitude_)) {
            if (test_collision(forward_position_, lateral_position_,
                    previous_altitude_, previous_altitude_) ||
                previous_altitude_ < 25) {
                begin_crash(CrashCause::Collision);
                return;
            }
            altitude_ = 25;
            vertical_velocity_ = 0;
            begin_crash(CrashCause::Fall);
            return;
        }
        lateral_position_ = previous_lateral_position_;
        lateral_step_ = 0;
        lateral_velocity_ = 0;
        forward_speed_ = forward_speed_ < 21 ? 0 : forward_speed_ - 20;
    }
    lateral_position_ = std::min(lateral_position_, 0x16d);

    if (altitude_ == 9 && surface_tile_ == 0u) {
        if (left_tile_ == 0u && right_tile_ < 0x0fu) lateral_position_ -= 0x0b;
        if (right_tile_ == 0u && left_tile_ < 0x0fu) lateral_position_ += 0x0b;
    }

    const bool was_dying = dying_;
    const int effect_result = apply_tile_effect(surface_tile_);
    if (effect_result != 0) {
        dying_ = true;
        if (!was_dying) death_ticks_ = 0u;
        if (effect_result == 2) {
            crash_cause_ = CrashCause::Slippery;
        }
        else if (fuel_ == 0) crash_cause_ = CrashCause::Fuel;
        else if (oxygen_ == 0) crash_cause_ = CrashCause::Oxygen;
        else crash_cause_ = CrashCause::OffRoad;
    }

    previous_forward_position_ = forward_position_;
    previous_lateral_position_ = lateral_position_;
    previous_altitude_ = altitude_;

    int steering = 0;
    if (left != right && !burning_) steering = left ? -1 : 1;
    int acceleration = 0;
    if (up != down) acceleration = up ? 5 : -5;
    int jump_velocity = 0;
    if (jump && altitude_ == 9 && surface_tile_ < 0x0fu) {
        fuel_ -= (jump_power_ * 0x1b) >> 2;
        jump_velocity = jump_power_;
        sound_tone(10, 520, 80);
    }

    const int lane_remainder = (lateral_position_ - 0x2a) % 0x32;
    const int row = forward_position_ >> 2;
    const auto obstruction_sample = [this, row](int lateral, int origin,
                                        int minimum, int maximum) {
        if (lateral < minimum || lateral >= maximum || row < 0 ||
            row >= static_cast<int>(kKosmonautRoadRows)) return std::uint8_t{0};
        const int lane = (lateral - origin) / 0x32;
        if (lane < 0 || lane >= static_cast<int>(kKosmonautRoadColumns)) {
            return std::uint8_t{0};
        }
        return road_tile(static_cast<unsigned>(row), static_cast<unsigned>(lane));
    };
    const auto ahead_left = obstruction_sample(
        lateral_position_, 0x34, 0x35, 0x162);
    const auto ahead_right = obstruction_sample(
        lateral_position_, 0x20, 0x21, 0x14e);
    const bool raised_obstruction = ahead_left >= 0x10u || ahead_right >= 0x10u;
    const bool tunnel_obstruction = ahead_left == 0x0fu || ahead_right == 0x0fu;
    const bool centered = lane_remainder > 0x0e && lane_remainder < 0x24;
    if (((!centered && raised_obstruction) || tunnel_obstruction) &&
        forward_speed_ < 0x32 && altitude_ == 9) {
        forward_speed_ = 0;
        forward_fraction_ = 0;
    }

    forward_speed_ += acceleration;
    const int maximum_speed = speed_penalty_active_ ? speed_penalty_ + 0xff : 0xff;
    forward_speed_ = std::clamp(forward_speed_, 0, maximum_speed);

    if (altitude_ == 9 && surface_tile_ < 0x0fu) {
        vertical_velocity_ = jump_velocity;
    }
    else {
        vertical_velocity_ -= 0x20;
    }
    if ((surface_tile_ == 0u || surface_tile_ == 0x10u) && dying_) {
        vertical_velocity_ = -10;
    }

    if (altitude_ < 0x28 && vertical_velocity_ >= 0 && !burning_ &&
        (vertical_velocity_ == 0 || steering != 0 || altitude_ == 9)) {
        lateral_step_ = ((forward_speed_ >> 5) + 1) * steering;
    }
    lateral_velocity_ = lateral_step_;
    lateral_position_ += lateral_step_;

    forward_fraction_ += forward_speed_;
    if (forward_fraction_ > 0xfe) {
        forward_position_ += forward_fraction_ / 0xff;
        forward_fraction_ %= 0xff;
    }

    if (altitude_ > 0x27 && vertical_velocity_ > 0) {
        if (!speed_penalty_active_) find_landing_adjustment();
        speed_penalty_active_ = true;
    }
    if (altitude_ == 9 && speed_penalty_active_) {
        forward_speed_ -= speed_penalty_;
        speed_penalty_ = 0;
        speed_penalty_active_ = false;
    }

    bounce_height_ += bounce_velocity_ >> 3;
    if (bounce_height_ < 1 && bounce_velocity_ < 0 &&
        (surface_tile_ % 0x10u) != 0u) {
        bounce_height_ = 0;
        bounce_velocity_ /= -3;
        sound_tone(11, 260, 35);
    }
    if (bounce_height_ > 0) bounce_velocity_ -= 0x20;

    altitude_ += vertical_velocity_ >> 3;
    if (altitude_ < 10 && vertical_velocity_ < 0 && altitude_ > -11 &&
        (surface_tile_ % 0x10u) != 0u) {
        altitude_ = 9;
        bounce_velocity_ = -vertical_velocity_ / 3;
        vertical_velocity_ = 0;
        sound_tone(11, 260, 35);
    }
    grounded_ = altitude_ == 9 && vertical_velocity_ == 0;

    score_ = road_score_base_ + static_cast<std::uint32_t>(
        std::max(0, forward_position_ - 4));
    session_best_score_ = std::max(session_best_score_, score_);
    if (forward_position_ >= 0x2f0) {
        begin_complete();
        return;
    }
    if (forward_position_ >= 400 && !checkpoint_reached_ && !demo) {
        checkpoint_reached_ = true;
        saved_jump_power_ = jump_power_;
    }
    if (dying_) {
        ++death_ticks_;
        if (death_ticks_ > 0x22u) {
            begin_crash(crash_cause_);
            return;
        }
    }
    render();
}

void KosmonautGame::begin_complete() {
    if (demo_session_) {
        phase_ = Phase::RoadSelect;
        phase_ticks_ = 0;
        demo_session_ = false;
        schedule_music(2u);
        render();
        return;
    }
    score_ = road_score_base_ + 748u;
    road_score_base_ = score_;
    session_best_score_ = std::max(session_best_score_, score_);
    best_score_ = std::max(best_score_, score_);
    if (selected_road_ + 1u >= unlocked_roads_ && unlocked_roads_ < kKosmonautRoadCount) {
        ++unlocked_roads_;
    }
    selected_road_ = (selected_road_ + 1u) % kKosmonautRoadCount;
    save_progress();
    phase_ticks_ = 0;
    checkpoint_reached_ = false;
    if (selected_road_ == session_start_road_) {
        phase_ = Phase::AllComplete;
        sound_tone(20, 1040, 180);
        schedule_music(2u);
        render();
    }
    else {
        reset_road(false);
    }
}

void KosmonautGame::begin_crash(CrashCause cause) {
    phase_ = Phase::Crashed;
    phase_ticks_ = 0;
    crash_cause_ = cause;
    dying_ = true;
    sound_tone(21, 105, 240);
    render();
}

const char* KosmonautGame::crash_message() const {
    const bool alternate = (animation_tick_ & 1u) != 0u;
    switch (crash_cause_) {
    case CrashCause::Fall:
        return alternate ? "Don't climb anything you can't get down" :
            "Trying to catch low-flying UFOs?";
    case CrashCause::Fuel:
        return alternate ? "Fuelout" : "Out of fuel";
    case CrashCause::Oxygen:
        return alternate ? "You have forgotten your oxygen..." : "Out of oxygen";
    case CrashCause::Slippery:
        return alternate ? "Remember-light red slabs are burning!" :
            "You never learn!";
    case CrashCause::Collision:
        if (altitude_ < 9) {
            return alternate ? "Searching for rabbitholes?" :
                "You'd better stay on the road";
        }
        return alternate ? "Still playing a battering-ram?" :
            "..but the stone was stronger!";
    case CrashCause::Explosion:
    case CrashCause::OffRoad:
        return alternate ? "Still trying to ride under the road?" :
            "Into the next galaxy?";
    }
    return "Trying to catch low-flying UFOs?";
}

int KosmonautGame::crash_message_x() const {
    const bool alternate = (animation_tick_ & 1u) != 0u;
    switch (crash_cause_) {
    case CrashCause::Fall: return alternate ? 0x20 : 0x00;
    case CrashCause::Fuel: return alternate ? 0x82 : 0x77;
    case CrashCause::Oxygen: return alternate ? 0x1c : 0x6b;
    case CrashCause::Slippery: return alternate ? 0x08 : 0x63;
    case CrashCause::Collision:
        return alternate && altitude_ < 9 ? 0x38 : 0x30;
    case CrashCause::Explosion:
    case CrashCause::OffRoad: return alternate ? 0x12 : 0x4c;
    }
    return 0;
}

void KosmonautGame::sound_tone(
    unsigned effect, unsigned frequency, unsigned milliseconds) {
    if (!sound_enabled_ || frequency == 0u) return;
    KosmonautSound sound;
    sound.effect = 0x10000u + effect;
    sound.sample_rate = 11025u;
    const std::size_t count = sound.sample_rate * milliseconds / 1000u;
    sound.samples.resize(count);
    const unsigned period = std::max(2u, sound.sample_rate / frequency);
    for (std::size_t index = 0; index < count; ++index) {
        const int envelope = static_cast<int>((count - index) * 42u / std::max<std::size_t>(1u, count));
        sound.samples[index] = static_cast<std::uint8_t>(
            128 + ((index % period) < period / 2u ? envelope : -envelope));
    }
    pending_sound_ = std::move(sound);
}

void KosmonautGame::schedule_music(unsigned track) {
    if (!sound_enabled_) return;
    const auto data = kosmonaut_music();
    struct TrackLayout {
        std::size_t parameters;
        std::size_t frequencies;
        std::size_t durations;
    };
    const TrackLayout layout = track == 1u
        ? TrackLayout{0x0000u, 0x0002u, 0x0484u}
        : TrackLayout{0x0906u, 0x0908u, 0x0d4au};
    if (layout.durations + 1u >= data.size() ||
        layout.parameters + 1u >= data.size()) return;

    constexpr std::uint32_t sample_rate = 11025u;
    constexpr std::uint64_t pit_clock = 1193182u;
    constexpr std::uint64_t pit_divisor = 8192u;
    const unsigned tempo = data[layout.parameters];
    const unsigned release = data[layout.parameters + 1u];
    KosmonautSound sound;
    sound.effect = 0x20000u + track;
    sound.sample_rate = sample_rate;
    sound.loop = true;

    std::uint64_t elapsed_ticks = 0u;
    std::uint64_t elapsed_samples = 0u;
    for (std::size_t note = 0u;; ++note) {
        const auto frequency_offset = layout.frequencies + note * 2u;
        const auto duration_offset = layout.durations + note * 2u;
        if (frequency_offset + 1u >= data.size() ||
            duration_offset + 1u >= data.size()) break;
        const auto divisor = table_word(data, frequency_offset);
        if (divisor == 0u) break;
        const auto duration_word = table_word(data, duration_offset);
        const auto units = static_cast<unsigned>(duration_word & 0x1fffu);
        const auto duration_ticks = static_cast<unsigned>(
            units * (51u - std::min(tempo, 51u)) / 5u);
        const bool legato = (duration_word & 0x4000u) != 0u;
        const unsigned audible_ticks = !legato && duration_ticks >= release
            ? duration_ticks - release : duration_ticks;
        const auto note_start_ticks = elapsed_ticks;
        elapsed_ticks += duration_ticks;
        const auto end_samples = elapsed_ticks * sample_rate * pit_divisor /
            pit_clock;
        const auto audible_end_samples =
            (note_start_ticks + audible_ticks) * sample_rate * pit_divisor /
            pit_clock;

        bool high = true;
        std::uint64_t phase = 0u;
        const std::uint64_t half_period =
            static_cast<std::uint64_t>(divisor) * sample_rate;
        while (elapsed_samples < end_samples) {
            std::uint8_t sample = 128u;
            if (divisor > 2u && elapsed_samples < audible_end_samples) {
                sample = static_cast<std::uint8_t>(high ? 168u : 88u);
                phase += pit_clock;
                while (phase >= half_period) {
                    phase -= half_period;
                    high = !high;
                }
            }
            sound.samples.push_back(sample);
            ++elapsed_samples;
        }
    }
    pending_sound_ = std::move(sound);
}

void KosmonautGame::silence_sound() {
    KosmonautSound sound;
    sound.effect = 0x20000u;
    sound.sample_rate = 11025u;
    pending_sound_ = std::move(sound);
}

void KosmonautGame::reset_starfield() {
    for (auto& star : stars_) star = StarState{};
    const auto random = kosmonaut_star_rng();
    if (random.size() >= 16u) {
        std::copy_n(random.begin(), 16u, star_rng_state_.begin());
    }
    star_rng_value_ = 0u;
    star_frame_ = 0u;
    star_previous_forward_ = -1;
}

std::uint8_t KosmonautGame::next_star_random() {
    const auto constants = kosmonaut_star_rng();
    const auto index = static_cast<unsigned>(star_rng_value_ & 0x0fu);
    const auto addend = constants.size() >= 32u
        ? constants[16u + index] : std::uint8_t{0};
    star_rng_state_[index] = static_cast<std::uint8_t>(
        star_rng_state_[index] + addend);
    star_rng_value_ = star_rng_state_[index];
    return star_rng_value_;
}

void KosmonautGame::advance_starfield() {
    ++star_frame_;
    if (star_previous_forward_ == forward_position_) return;
    star_previous_forward_ = forward_position_;
    const auto motion = kosmonaut_star_motion();
    const auto pointers = kosmonaut_star_path_pointers();
    const auto paths = kosmonaut_star_paths();

    const auto update_group = [&](std::size_t first, std::size_t count) {
        for (std::size_t index = first;
            index < first + count && index < stars_.size(); ++index) {
            auto& star = stars_[index];
            if (star.path_offset >= paths.size() ||
                paths[star.path_offset] == 0u) {
                unsigned path = 0u;
                do path = next_star_random() & 0x3fu;
                while (path >= 55u);
                star.path_offset = table_word(pointers, path * 2u);
                star.screen_offset = 0x071b;
                star.spread = 0u;
                star.mask = 1u;
            }
            if (star.path_offset >= paths.size()) continue;
            const auto command = paths[star.path_offset++];
            if ((command & 0x08u) != 0u && star.spread != 0xffu) {
                ++star.spread;
            }
            const auto delta_offset = static_cast<std::size_t>(command >> 4u);
            star.screen_offset += static_cast<std::int16_t>(
                table_word(motion, delta_offset));
            star.mask = static_cast<std::uint8_t>(1u << (command & 7u));
        }
    };

    if ((star_frame_ & 1u) == 0u) update_group(0u, 20u);
    if ((star_frame_ & 3u) == 1u) update_group(20u, 40u);
    if ((star_frame_ & 7u) == 3u) update_group(60u, 60u);
}

void KosmonautGame::draw_starfield() {
    const auto draw_group = [this](std::size_t first, std::size_t count) {
        const auto half = count / 2u;
        for (std::size_t local = 0u; local < count; ++local) {
            const auto& star = stars_[first + local];
            const bool right = local >= half;
            const int destination = star.screen_offset + (right
                ? static_cast<int>(star.spread) + 1
                : -static_cast<int>(star.spread));
            if (destination < 0) continue;
            const int byte_x = destination % 40;
            const int y = destination / 40;
            if (byte_x < 0 || byte_x >= 40 || y < 0 || y >= 139) continue;
            unsigned bit = 0u;
            if (right) {
                auto value = star.mask;
                while (value > 1u) {
                    value >>= 1u;
                    ++bit;
                }
            }
            else {
                while (bit < 8u &&
                    (star.mask & (0x80u >> bit)) == 0u) ++bit;
            }
            if (bit < 8u) put_pixel(pixels_, byte_x * 8 + bit, y, 9u);
        }
    };
    draw_group(0u, 20u);
    draw_group(20u, 40u);
    draw_group(60u, 60u);
}

bool KosmonautGame::tick(const NativeInput& input) {
    if (input.editor_save_pressed) {
        sound_enabled_ = !sound_enabled_;
        if (!sound_enabled_) silence_sound();
        else if (phase_ == Phase::Tutorial) schedule_music(1u);
        else if (phase_ == Phase::RoadSelect || phase_ == Phase::NameEntry) {
            schedule_music(2u);
        }
    }
    switch (phase_) {
    case Phase::Title:
        if (input.escape_pressed) {
            silence_sound();
            return true;
        }
        if (input.enter_pressed || input.jump || input.kosmonaut_demo_pressed ||
            input.left || input.right || input.up || input.down ||
            input.editor_play_pressed || input.editor_save_pressed ||
            input.text_character != 0u) {
            /* 1000:3988 leaves the completed title frame visible for six
               timing units before replacing it with the tutorial page. */
            phase_ = Phase::TitleExit;
            phase_ticks_ = 0;
        }
        else {
            ++phase_ticks_;
            if (phase_ticks_ >= 700u) {
                phase_ = Phase::TitleExit;
                phase_ticks_ = 0u;
            }
            if (phase_ == Phase::Title) render();
        }
        break;
    case Phase::TitleExit:
        if (++phase_ticks_ >= 6u) {
            phase_ = Phase::Tutorial;
            phase_ticks_ = 0u;
            tutorial_pass_ = 0u;
            forward_position_ = 4;
            schedule_music(1u);
            render();
        }
        break;
    case Phase::Tutorial:
        ++phase_ticks_;
        if (input.escape_pressed) {
            silence_sound();
            return true;
        }
        if (input.kosmonaut_demo_pressed) {
            reset_road(true);
        }
        else if (input.enter_pressed || input.jump) {
            phase_ = Phase::RoadSelect;
            phase_ticks_ = 0u;
            schedule_music(2u);
            render();
        }
        else {
            ++forward_position_;
            if (forward_position_ >= 0x274) {
                ++tutorial_pass_;
                forward_position_ = 4;
                if (tutorial_pass_ >= 2u) reset_road(true);
            }
            render();
        }
        break;
    case Phase::RoadSelect:
        if (input.escape_pressed) {
            silence_sound();
            return true;
        }
        if (input.down) {
            selected_road_ = selected_road_ == 0u ? unlocked_roads_ - 1u : selected_road_ - 1u;
            render();
        }
        else if (input.up) {
            selected_road_ = (selected_road_ + 1u) % unlocked_roads_;
            render();
        }
        else if (input.enter_pressed || input.jump) {
            score_ = 0u;
            road_score_base_ = 0u;
            session_best_score_ = 0u;
            session_start_road_ = selected_road_;
            idle_ticks_ = 0u;
            session_had_input_ = false;
            checkpoint_reached_ = false;
            reset_road(false);
        }
        break;
    case Phase::Playing:
        if (input.escape_pressed) {
            finish_session();
            render();
        }
        else if (input.editor_play_pressed) {
            phase_ = Phase::Paused;
            render();
        }
        else update_playing(input, false);
        break;
    case Phase::Demo:
        if (input.escape_pressed) {
            silence_sound();
            return true;
        }
        if (input.enter_pressed || input.jump) {
            phase_ = Phase::RoadSelect;
            demo_session_ = false;
            schedule_music(2u);
            render();
        }
        else update_playing(input, true);
        break;
    case Phase::Paused:
        if (input.escape_pressed) {
            finish_session();
            render();
        }
        else if (input.editor_play_pressed || input.enter_pressed || input.jump ||
            input.left || input.right || input.up || input.down ||
            input.editor_save_pressed || input.kosmonaut_demo_pressed ||
            input.text_character != 0u) {
            phase_ = Phase::Playing;
            render();
        }
        break;
    case Phase::RoadComplete:
        reset_road(false);
        break;
    case Phase::AllComplete:
        ++phase_ticks_;
        if (phase_ticks_ > 250u || input.enter_pressed || input.escape_pressed ||
            input.jump) {
            finish_session();
        }
        break;
    case Phase::Crashed:
        if (phase_ticks_ < 34u) {
            ++phase_ticks_;
            render();
        }
        else if (input.escape_pressed) {
            if (demo_session_) {
                phase_ = Phase::RoadSelect;
                demo_session_ = false;
            }
            else {
                finish_session();
            }
            render();
        }
        else {
            ++phase_ticks_;
            if (phase_ticks_ >= 84u) {
                if (demo_session_) {
                    phase_ = Phase::Tutorial;
                    demo_session_ = false;
                    phase_ticks_ = 0u;
                    tutorial_pass_ = 0u;
                    forward_position_ = 4;
                    schedule_music(1u);
                    render();
                }
                else if (session_had_input_) {
                    reset_road(false);
                }
                else {
                    finish_session();
                    render();
                }
            }
            /* The DOS path draws the crash text once, silences the speaker,
               and waits 50 timing units on that frozen page. Redrawing the
               playfield here made the supposedly static message look as if
               it were clearing and reappearing against moving scenery. */
        }
        break;
    case Phase::NameEntry:
        if (input.backspace_pressed && name_entry_slot_ < high_scores_.size() &&
            !high_scores_[name_entry_slot_].name.empty()) {
            high_scores_[name_entry_slot_].name.pop_back();
            render();
        }
        else if (input.text_character != 0u && name_entry_slot_ < high_scores_.size() &&
            high_scores_[name_entry_slot_].name.size() < 24u) {
            high_scores_[name_entry_slot_].name.push_back(
                static_cast<char>(input.text_character));
            render();
        }
        if (input.enter_pressed) {
            save_progress();
            phase_ = Phase::RoadSelect;
            render();
        }
        break;
    }
    return false;
}

void KosmonautGame::finish_session() {
    const auto final_score = static_cast<std::uint16_t>(
        std::min<std::uint32_t>(
            std::max(score_, session_best_score_), 0xffffu));
    unsigned slot = 0u;
    while (slot < high_scores_.size() && high_scores_[slot].score > final_score) ++slot;
    if (slot < high_scores_.size() && final_score != 0u) {
        for (unsigned index = static_cast<unsigned>(high_scores_.size() - 1u);
            index > slot; --index) {
            high_scores_[index] = high_scores_[index - 1u];
        }
        high_scores_[slot] = HighScore{"", final_score,
            static_cast<std::uint8_t>(selected_road_ + 1u)};
        name_entry_slot_ = slot;
        best_score_ = std::max<std::uint32_t>(best_score_, final_score);
        phase_ = Phase::NameEntry;
    }
    else {
        phase_ = Phase::RoadSelect;
    }
    schedule_music(2u);
    save_progress();
}

void KosmonautGame::render_title() {
    const auto screens = kosmonaut_screens();
    std::copy_n(screens.begin() + 3u * kScreenBytes, kScreenBytes, pixels_.begin());
    draw_text(pixels_, 0x10, 0x6e, "Copyright * 1990 by TIW Systems,Inc.", 0u);
    draw_text(pixels_, 0x32, 0x78, "All rights reserved", 0u);
    draw_text(pixels_, 0x32, 0x82, "Programmed by Jaan Tallinn", 0u);
    draw_text(pixels_, 0xa2, 0x8c, "Ahti Heinla", 0u);
    draw_text(pixels_, 0xa2, 0x96, "Priit Kasesalu", 0u);
    draw_text(pixels_, 0x32, 0xa0, "Graphics by Kaspar P. Loit", 0u);
    draw_text(pixels_, 0x32, 0xaa, "Music by J#ri Tallinn", 0u);
    draw_scroll(pixels_, kosmonaut_title_scroll(), phase_ticks_, 1u, 4u);
}

void KosmonautGame::render_high_scores() {
    const auto screens = kosmonaut_screens();
    std::copy_n(screens.begin(), kScreenBytes, pixels_.begin());
    for (unsigned index = 0; index < high_scores_.size(); ++index) {
        const auto& entry = high_scores_[index];
        if (entry.score == 0u) continue;
        const int y = static_cast<int>(index * 10u + 0x0cu);
        draw_text(pixels_, 0x10, y, entry.name, 0u);
        if (index == 0u && entry.score >= 19448u) {
            draw_text(pixels_, 0xe6, y, "Starmaster", 0u);
        }
        else {
            unsigned score = entry.score;
            for (int digit = 4; digit >= 0; --digit) {
                draw_char(pixels_, 0xe6 + digit * 10, y,
                    static_cast<char>('0' + score % 10u), 0u);
                score /= 10u;
            }
            draw_text(pixels_, 0x122, y, std::to_string(entry.road), 0u);
        }
    }
    draw_text(pixels_, 0x37, 1, "Name                  Score Road", 0u);
    draw_text(pixels_, 0x10, 0x70, "Press Esc to quit,", 0u);
    draw_char(pixels_, 0xc0, 0x70, static_cast<char>(0x18), 0u);
    draw_char(pixels_, 0xc8, 0x70, static_cast<char>(0x19), 0u);
    draw_text(pixels_, 0xd0, 0x70, " to change road", 0u);
    draw_text(pixels_, 0x10, 0x7a, "       or space to play road  ", 0u);
    draw_text(pixels_, 0xf7, 0x7a,
        padded_number(selected_road_ + 1u, 2u), 0u);
}

void KosmonautGame::render_road_select() {
    render_high_scores();
}

void KosmonautGame::render_ship(
    const std::vector<std::uint8_t>* occlusion) {
    const auto sprites = kosmonaut_ship_sprites();
    constexpr std::array<unsigned, 5> animation{0u, 1u, 2u, 1u, 0u};
    int lateral_view = 0;
    if (lateral_position_ >= 0x28) {
        lateral_view = lateral_position_ < 0x155
            ? (lateral_position_ - 0x28) / 0x2b
            : 6;
    }
    int vertical_view = 0;
    if (jump_power_ >= 0x3c && vertical_velocity_ != 0) {
        vertical_view = vertical_velocity_ < 0 ? 8 : 4;
    }
    unsigned frame = std::min<unsigned>(97u,
        14u + animation[ship_animation_ % animation.size()] +
        static_cast<unsigned>(vertical_view + lateral_view * 12));
    const bool destruction = phase_ == Phase::Crashed && phase_ticks_ < 34u &&
        (crash_cause_ == CrashCause::Fall ||
         crash_cause_ == CrashCause::Collision ||
         crash_cause_ == CrashCause::Explosion);
    if (destruction) {
        const auto frames = kosmonaut_destruction_frames();
        frame = std::min<unsigned>(13u, table_word(
            frames, std::min<unsigned>(phase_ticks_, 33u) * 2u));
    }
    const std::size_t frame_offset = static_cast<std::size_t>(frame) * 26u * 9u;
    if (frame_offset + 26u * 9u > sprites.size()) return;

    const int renderer_lateral =
        destruction && phase_ticks_ >= 32u ? 5 : lateral_position_ - 5;
    int source_column = 0;
    int screen_x = renderer_lateral - 0x28;
    if (screen_x < 0) {
        source_column = -screen_x;
        screen_x = 0;
    }
    int width = 26 - source_column;
    if (screen_x >= 0x126) width -= screen_x - 0x126;
    if (width <= 0) return;
    const int screen_y = 112 - altitude_;
    for (int column = 0; column < width; ++column) {
        for (int row = 0; row < 9; ++row) {
            const auto color = sprites[frame_offset +
                static_cast<std::size_t>(source_column + column) * 9u + row];
            const int x = screen_x + column;
            const int y = screen_y + row;
            if (color == 0u || x < 0 || x >= static_cast<int>(kWidth) ||
                y < 0 || y >= static_cast<int>(kHeight)) continue;
            const auto destination = static_cast<std::size_t>(y) * kWidth + x;
            if (occlusion != nullptr && destination < occlusion->size() &&
                (*occlusion)[destination] != 0u) continue;
            put_pixel(pixels_, x, y, color);
        }
    }
}

void KosmonautGame::render_original_hud() {
    if (!demo_session_) {
        draw_line(pixels_, 0, 3, 319, 3, 0u);
        draw_line(pixels_, 0x17, 2, 0x9d, 2, 0x0du);
        draw_line(pixels_, 0x17, 4, 0x9d, 4, 0x0du);
        draw_line(pixels_, 0x17, 3, 0x17, 3, 0x0du);
        draw_line(pixels_, 0x9d, 3, 0x9d, 3, 0x0du);
        draw_line(pixels_, 0xb2, 2, 0x129, 2, 0x0du);
        draw_line(pixels_, 0xb2, 4, 0x129, 4, 0x0du);
        draw_line(pixels_, 0xb2, 3, 0xb2, 3, 0x0du);
        draw_line(pixels_, 0x129, 3, 0x129, 3, 0x0du);
        draw_line(pixels_, 0x17, 1, 0x9e, 1, 8u);
        draw_line(pixels_, 0x9e, 2, 0x9e, 4, 8u);
        draw_line(pixels_, 0xb2, 1, 0x12a, 1, 8u);
        draw_line(pixels_, 0x12a, 2, 0x12a, 4, 8u);
        draw_line(pixels_, 0x16, 5, 0x9e, 5, 0x0fu);
        draw_line(pixels_, 0x16, 1, 0x16, 4, 0x0fu);
        draw_line(pixels_, 0xb1, 5, 0x12a, 5, 0x0fu);
        draw_line(pixels_, 0xb1, 1, 0xb1, 5, 0x0fu);
        if (forward_position_ > 4 && forward_position_ < 401) {
            draw_line(pixels_, 0x18, 3,
                std::min(0x9c, forward_position_ / 3 + 0x17), 3, 7u);
        }
        if (forward_position_ > 400) {
            draw_line(pixels_, 0xb3, 3,
                std::min(0x129, (forward_position_ - 400) / 3 + 0xb3), 3, 7u);
        }
        draw_text(pixels_, 0x7d, 0x37, " Road ", 0u);
        draw_text(pixels_, 0xad, 0x37,
            padded_number(selected_road_ + 1u, 2u), 0u);
    }

    const auto tables = kosmonaut_simulation_tables();
    const int effective_speed = speed_penalty_active_
        ? forward_speed_ - speed_penalty_ : forward_speed_;
    const unsigned speed_index = static_cast<unsigned>(
        std::clamp(effective_speed / 10, 0, 25));
    draw_line(pixels_, 0x76, 0xac,
        table_word(tables, speed_index * 2u),
        table_word(tables, 0x34u + speed_index * 2u), 0x0au);

    const unsigned oxygen_index = static_cast<unsigned>(
        std::clamp((oxygen_ >> 8) / 5, 0, 25));
    draw_line(pixels_, 0xbc, 0xac,
        table_word(tables, oxygen_index * 2u) + 0x46,
        table_word(tables, 0x34u + oxygen_index * 2u), 0x0au);

    const unsigned fuel_index = static_cast<unsigned>(
        std::clamp(fuel_ >> 12, 0, 7));
    draw_line(pixels_, 0x99, 0xa0,
        table_word(tables, 0x68u + fuel_index * 2u),
        table_word(tables, 0x78u + fuel_index * 2u), 0x0au);

    const int jump_bars = std::clamp(jump_power_ / 9, 0, 13);
    for (int bar = 0; bar < 13; ++bar) {
        const int y = bar * -3 + 0xbe;
        draw_line(pixels_, 0xd8, y, 0xdc, y,
            static_cast<std::uint8_t>(bar < jump_bars ? 6u : 0x0fu));
        draw_line(pixels_, 0xdb, y - 1, 0xdc, y - 1,
            static_cast<std::uint8_t>(bar < jump_bars ? 6u : 0x0fu));
        draw_line(pixels_, 0xd8, y - 1, 0xdb, y - 1,
            static_cast<std::uint8_t>(bar < jump_bars ? 7u : 0x0du));
    }

    unsigned displayed_score = static_cast<unsigned>(std::min<std::uint32_t>(
        99999u, road_score_base_ + static_cast<std::uint32_t>(
            std::max(0, forward_position_ - 4))));
    for (int digit = 0; digit < 5; ++digit) {
        draw_glyph(pixels_, (4 - digit) * 4 + 0x90, 0xbc,
            static_cast<int>(displayed_score % 10u + 1u), 2u);
        displayed_score /= 10u;
    }
}

void KosmonautGame::render_playfield() {
    const auto screens = kosmonaut_screens();
    const bool tutorial = phase_ == Phase::Tutorial;
    std::copy_n(screens.begin() + (tutorial ? 2u : 1u) * kScreenBytes,
        kScreenBytes, pixels_.begin());
    advance_starfield();
    if (!tutorial) draw_starfield();
    const auto tutorial_background = tutorial ? pixels_ : std::vector<std::uint8_t>{};
    if (tutorial) std::fill(pixels_.begin(), pixels_.end(), 0xffu);
    const auto masks = kosmonaut_render_masks();
    const auto pointers = kosmonaut_render_pointers();
    const auto shapes = kosmonaut_render_shapes();

    /* The original renderer reaches the craft at a depth-dependent cell in
       its two road passes. Record only geometry drawn after that depth, then
       composite the sprite once through that mask. This is equivalent to the
       EGA write ordering while preventing ordinary floor spans from one
       half-pass clipping a grounded craft at the center seam. */
    const bool ship_visible = !tutorial &&
        !(phase_ == Phase::Crashed && phase_ticks_ >= 34u &&
          (crash_cause_ == CrashCause::Fall ||
           crash_cause_ == CrashCause::Collision ||
           crash_cause_ == CrashCause::Explosion));
    int ship_cell = (lateral_position_ + 0x16) / 0x32;
    if (ship_cell > 3) {
        const int alternate_cell = (lateral_position_ + 3) / 0x32;
        if (alternate_cell == 3) --ship_cell;
        else ship_cell = 0x28 - alternate_cell;
    }
    int ship_draw_countdown = ship_cell + 0x15;
    if (altitude_ >= -7) {
        const int altitude_cell = ship_cell + (altitude_ >= 9 ? 0x15 : 0x14);
        ship_draw_countdown = altitude_cell +
            (altitude_ >= 0x18 || (forward_position_ & 3) == 0 ? 6 : 3);
    }
    bool ship_depth_reached = false;
    bool geometry_occludes_ship = false;
    std::vector<std::uint8_t> ship_occlusion(
        ship_visible ? kScreenBytes : 0u, 0u);

    auto map_at = [this](int offset) -> std::uint8_t {
        int row = offset / static_cast<int>(kKosmonautRoadColumns);
        int lane = offset % static_cast<int>(kKosmonautRoadColumns);
        if (lane < 0) {
            lane += static_cast<int>(kKosmonautRoadColumns);
            --row;
        }
        if (row < 0 || row >= static_cast<int>(kKosmonautRoadRows)) return 0u;
        return road_tile(static_cast<unsigned>(row), static_cast<unsigned>(lane));
    };
    auto write_mask = [this, tutorial, &ship_depth_reached,
                          &geometry_occludes_ship, &ship_occlusion](
                          int destination, std::uint8_t mask, std::uint8_t color) {
        if (destination < 0) return;
        const int raw_y = destination / 40;
        const int byte_x = destination % 40;
        const int y = raw_y + (tutorial ? 0 : 58);
        if (byte_x < 0 || byte_x >= 40 || y < 0 || y >= 139) return;
        for (int bit = 0; bit < 8; ++bit) {
            if ((mask & (0x80u >> bit)) != 0u) {
                const int x = byte_x * 8 + bit;
                if (ship_depth_reached && geometry_occludes_ship) {
                    ship_occlusion[static_cast<std::size_t>(y) * kWidth + x] = 1u;
                }
                put_pixel(pixels_, x, y, color);
            }
        }
    };
    auto draw_program = [&](std::size_t& cursor, unsigned pass,
                            std::optional<std::uint8_t> first_color) {
        while (cursor < shapes.size()) {
            auto color = shapes[cursor++];
            if (color == 0xffu) return;
            if (color == 0xfeu) continue;
            if (cursor + 3u >= shapes.size()) return;
            if (first_color) {
                color = *first_color;
                first_color.reset();
            }
            else if (color == 0x10u) color = static_cast<std::uint8_t>(14u + pass);
            const int base = shapes[cursor] | (shapes[cursor + 1u] << 8u);
            cursor += 2u;
            const int start = shapes[cursor++];
            int destination = pass == 0u ? base - start - 1 : base + start;
            const int direction = pass == 0u ? 1 : -1;
            while (cursor + 3u < shapes.size()) {
                const auto left = shapes[cursor];
                const auto width = shapes[cursor + 1u];
                const auto right = shapes[cursor + 2u];
                const auto step = shapes[cursor + 3u];
                write_mask(destination, masks[pass * 256u + left], color);
                destination += direction;
                for (unsigned byte = 0; byte < width; ++byte) {
                    write_mask(destination, 0xffu, color);
                    destination += direction;
                }
                write_mask(destination, masks[pass * 256u + right], color);
                destination += direction;
                if (pass == 0u) {
                    destination += step < 0x14u
                        ? 38 - width - step
                        : 78 - width - step;
                }
                else {
                    destination += step < 0x14u
                        ? 42 + width + step
                        : 2 + width + step;
                }
                cursor += 4u;
                if (step == 0xffu) return;
                if (step == 0xfeu) break;
            }
        }
    };
    auto skip_block = [&](std::size_t& cursor) {
        if (cursor >= shapes.size() || shapes[cursor] == 0xffu) return;
        cursor += 3u;
        while (cursor < shapes.size()) {
            cursor += 4u;
            if (cursor >= shapes.size()) return;
            if (shapes[cursor] == 0xffu) {
                ++cursor;
                return;
            }
        }
    };
    auto pointer_at = [&](unsigned byte_offset) -> std::size_t {
        if (byte_offset + 1u >= pointers.size()) return 0u;
        return static_cast<std::size_t>(
            pointers[byte_offset] | (pointers[byte_offset + 1u] << 8u));
    };

    unsigned descriptor = static_cast<unsigned>(forward_position_ & 3) * 0xc6u;
    int road_cursor = (forward_position_ >> 2) * 6 + 0x30;
    for (unsigned pass = 0; pass < 2u; ++pass) {
        for (unsigned row = 0; row < 11u; ++row) {
            for (unsigned lane = 0; lane < 3u; ++lane) {
                const auto tile = map_at(road_cursor);
                geometry_occludes_ship = altitude_ < 9 || tile >= 0x0fu;
                const int direction = pass == 0u ? 1 : -1;
                const auto behind = map_at(road_cursor - 6);
                const auto outward = map_at(road_cursor + direction);
                const auto behind_outward = map_at(road_cursor - 6 + direction);
                if ((tile & 0x0fu) != 0u) {
                    auto cursor = pointer_at(descriptor);
                    bool skipped = false;
                    if (tile == 0x0fu || (tile > 0x0eu && behind > 0x0eu)) {
                        skip_block(cursor);
                    }
                    else if (cursor >= shapes.size() || shapes[cursor] >= 0xfeu) {
                        skipped = true;
                    }
                    else {
                        draw_program(cursor, pass,
                            static_cast<std::uint8_t>(tile & 0x0fu));
                    }
                    if (!skipped &&
                        (behind_outward == 0x0eu ||
                         (behind_outward < 0x0eu &&
                          (outward & 0x0fu) == 0u) || behind == 0u)) {
                        draw_program(cursor, pass, std::nullopt);
                    }
                }
                if (tile >= 0x0fu) {
                    auto cursor = pointer_at(descriptor + 2u);
                    if (behind >= 0x0fu) skip_block(cursor);
                    else draw_program(cursor, pass, std::nullopt);
                    draw_program(cursor, pass, std::nullopt);
                    if (outward < 0x0fu) draw_program(cursor, pass, std::nullopt);
                    if (behind < 0x0fu) {
                        cursor = pointer_at(descriptor + 4u);
                        draw_program(cursor, pass, std::nullopt);
                        if (tile == 0x0fu) draw_program(cursor, pass, std::nullopt);
                    }
                }
                if (ship_visible && !ship_depth_reached &&
                    --ship_draw_countdown == 0) {
                    ship_depth_reached = true;
                }
                descriptor += 6u;
                road_cursor += pass == 0u ? 1 : -1;
            }
            road_cursor -= pass == 0u ? 9 : 3;
        }
        descriptor -= 0xc6u;
        road_cursor += 0x47;
    }
    if (tutorial) {
        const auto rendered = pixels_;
        pixels_ = tutorial_background;
        constexpr int kTutorialSkyBottom = 82;
        for (int y = 0; y <= kTutorialSkyBottom; ++y) {
            for (int x = 0; x < static_cast<int>(kWidth); ++x) {
                const auto source = static_cast<std::size_t>(y) * kWidth + x;
                if (rendered[source] == 0xffu) continue;
                const auto destination = static_cast<std::size_t>(
                    kTutorialSkyBottom - y) * kWidth + x;
                pixels_[destination] = rendered[source];
            }
        }
    }
    if (ship_visible) {
        render_ship(ship_depth_reached ? &ship_occlusion : nullptr);
    }
    if (!tutorial) render_original_hud();
}

void KosmonautGame::render_status(const char* text, std::uint8_t color) {
    render_playfield();
    (void)color;
    draw_centered(pixels_, 0x37, text, 0u);
}

void KosmonautGame::render() {
    switch (phase_) {
    case Phase::Title:
        render_title();
        break;
    case Phase::TitleExit:
        /* Preserve the final title page exactly as the six-tick DOS wait did. */
        break;
    case Phase::Tutorial: {
        render_playfield();
        fill_rect(pixels_, 0, 0xc0, static_cast<int>(kWidth),
            static_cast<int>(kHeight), 0u);
        draw_scroll(pixels_, kosmonaut_tutorial_scroll(), phase_ticks_, 2u, 3u);
        break;
    }
    case Phase::RoadSelect:
        render_road_select();
        break;
    case Phase::Playing:
    case Phase::Demo:
        render_playfield();
        break;
    case Phase::Paused:
        render_playfield();
        draw_text(pixels_, 0x82, 0x41, "Pausing", 0u);
        break;
    case Phase::RoadComplete:
        render_status("ROAD COMPLETE", 0x0eu);
        break;
    case Phase::AllComplete: {
        const auto screens = kosmonaut_screens();
        std::copy_n(screens.begin() + 4u * kScreenBytes, kScreenBytes, pixels_.begin());
        break;
    }
    case Phase::Crashed:
        if (phase_ticks_ < 34u) render_playfield();
        else {
            render_playfield();
            draw_text(pixels_, crash_message_x(), 0x37, crash_message(), 0u);
        }
        break;
    case Phase::NameEntry:
        render_high_scores();
        draw_centered(pixels_, 0x75, "Enter your name", 0u);
        break;
    }
}

const std::vector<std::uint8_t>& KosmonautGame::pixels() const {
    return pixels_;
}

const std::array<std::uint8_t, 256u * 3u>& KosmonautGame::palette() const {
    return palette_;
}

std::optional<KosmonautSound> KosmonautGame::consume_sound() {
    auto result = std::move(pending_sound_);
    pending_sound_.reset();
    return result;
}

unsigned KosmonautGame::selected_road() const {
    return selected_road_;
}

unsigned KosmonautGame::unlocked_roads() const {
    return unlocked_roads_;
}

bool KosmonautGame::playing() const {
    return phase_ == Phase::Playing || phase_ == Phase::Demo ||
        phase_ == Phase::Paused;
}

} // namespace skyroads
