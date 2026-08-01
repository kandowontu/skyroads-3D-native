#pragma once

#include "data.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace skyroads {

constexpr int kScreenWidth = 320;
constexpr int kScreenHeight = 200;

[[nodiscard]] constexpr std::uint32_t rgb(Rgb color) {
    return (static_cast<std::uint32_t>(color.r) << 16U) |
           (static_cast<std::uint32_t>(color.g) << 8U) |
           static_cast<std::uint32_t>(color.b);
}

class Canvas {
public:
    Canvas();

    void clear(std::uint32_t color);
    void pixel(int x, int y, std::uint32_t color);
    void rectangle(int x, int y, int width, int height, std::uint32_t color);
    void line(int x0, int y0, int x1, int y1, std::uint32_t color);
    void trapezoid(
        float far_left,
        float far_right,
        float far_y,
        float near_left,
        float near_right,
        float near_y,
        std::uint32_t color);
    void blit(
        const ImageFrame& frame,
        const ImagePalette& palette,
        int x,
        int y,
        bool transparent_zero = true);
    void blit_rotated_sprite(
        const ImageFrame& strip,
        const ImagePalette& palette,
        std::size_t sprite_index,
        int x,
        int y,
        bool rotate_clockwise);
    void text(int x, int y, const std::string& value, std::uint32_t color, int scale = 1);

    [[nodiscard]] const std::vector<std::uint32_t>& pixels() const { return pixels_; }
    [[nodiscard]] std::vector<std::uint32_t>& pixels() { return pixels_; }

private:
    std::vector<std::uint32_t> pixels_;
};

struct Input {
    bool left{};
    bool right{};
    bool accelerate{};
    bool brake{};
    bool jump_pressed{};
    bool confirm_pressed{};
    bool back_pressed{};
    bool previous_level_pressed{};
    bool next_level_pressed{};
    bool restart_pressed{};
};

enum class AudioCue {
    None,
    Intro,
    Jump,
    Crash,
    Complete,
};

enum class GameMode {
    Title,
    Playing,
    Crashed,
    Complete,
};

class Game {
public:
    explicit Game(Assets assets);

    void update(const Input& input, double seconds);
    void render(Canvas& canvas) const;
    void start_level(std::size_t archive_index);

    [[nodiscard]] AudioCue consume_audio_cue();
    [[nodiscard]] bool quit_requested() const { return quit_requested_; }
    [[nodiscard]] GameMode mode() const { return mode_; }
    [[nodiscard]] std::size_t level_index() const { return level_index_; }
    [[nodiscard]] double distance() const { return distance_; }
    [[nodiscard]] double speed() const { return speed_; }
    [[nodiscard]] const Assets& assets() const { return assets_; }

private:
    void reset_level();
    void crash();
    void render_title(Canvas& canvas) const;
    void render_game(Canvas& canvas) const;
    void render_road(Canvas& canvas, const Road& road) const;
    void render_ship(Canvas& canvas) const;
    [[nodiscard]] std::uint16_t current_cell() const;

    Assets assets_;
    GameMode mode_{GameMode::Title};
    std::size_t level_index_{1};
    double distance_{};
    double speed_{};
    double lane_{};
    double altitude_{};
    double vertical_velocity_{};
    double fuel_{};
    double oxygen_{};
    double state_time_{};
    std::uint64_t ticks_{};
    bool airborne_{};
    bool quit_requested_{};
    AudioCue audio_cue_{AudioCue::Intro};
};

} // namespace skyroads
