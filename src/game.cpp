#include "game.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>

namespace skyroads {
namespace {

using Glyph = std::array<std::uint8_t, 7>;

Glyph glyph(char c) {
    switch (c) {
    case 'A': return {14, 17, 17, 31, 17, 17, 17};
    case 'B': return {30, 17, 17, 30, 17, 17, 30};
    case 'C': return {14, 17, 16, 16, 16, 17, 14};
    case 'D': return {30, 17, 17, 17, 17, 17, 30};
    case 'E': return {31, 16, 16, 30, 16, 16, 31};
    case 'F': return {31, 16, 16, 30, 16, 16, 16};
    case 'G': return {14, 17, 16, 23, 17, 17, 15};
    case 'H': return {17, 17, 17, 31, 17, 17, 17};
    case 'I': return {14, 4, 4, 4, 4, 4, 14};
    case 'J': return {7, 2, 2, 2, 18, 18, 12};
    case 'K': return {17, 18, 20, 24, 20, 18, 17};
    case 'L': return {16, 16, 16, 16, 16, 16, 31};
    case 'M': return {17, 27, 21, 21, 17, 17, 17};
    case 'N': return {17, 25, 21, 19, 17, 17, 17};
    case 'O': return {14, 17, 17, 17, 17, 17, 14};
    case 'P': return {30, 17, 17, 30, 16, 16, 16};
    case 'Q': return {14, 17, 17, 17, 21, 18, 13};
    case 'R': return {30, 17, 17, 30, 20, 18, 17};
    case 'S': return {15, 16, 16, 14, 1, 1, 30};
    case 'T': return {31, 4, 4, 4, 4, 4, 4};
    case 'U': return {17, 17, 17, 17, 17, 17, 14};
    case 'V': return {17, 17, 17, 17, 17, 10, 4};
    case 'W': return {17, 17, 17, 21, 21, 21, 10};
    case 'X': return {17, 17, 10, 4, 10, 17, 17};
    case 'Y': return {17, 17, 10, 4, 4, 4, 4};
    case 'Z': return {31, 1, 2, 4, 8, 16, 31};
    case '0': return {14, 17, 19, 21, 25, 17, 14};
    case '1': return {4, 12, 4, 4, 4, 4, 14};
    case '2': return {14, 17, 1, 2, 4, 8, 31};
    case '3': return {30, 1, 1, 14, 1, 1, 30};
    case '4': return {2, 6, 10, 18, 31, 2, 2};
    case '5': return {31, 16, 16, 30, 1, 1, 30};
    case '6': return {14, 16, 16, 30, 17, 17, 14};
    case '7': return {31, 1, 2, 4, 8, 8, 8};
    case '8': return {14, 17, 17, 14, 17, 17, 14};
    case '9': return {14, 17, 17, 15, 1, 1, 14};
    case ':': return {0, 4, 4, 0, 4, 4, 0};
    case '.': return {0, 0, 0, 0, 0, 4, 4};
    case '-': return {0, 0, 0, 31, 0, 0, 0};
    case '[': return {14, 8, 8, 8, 8, 8, 14};
    case ']': return {14, 2, 2, 2, 2, 2, 14};
    case '/': return {1, 2, 2, 4, 8, 8, 16};
    case '!': return {4, 4, 4, 4, 4, 0, 4};
    default: return {};
    }
}

std::uint32_t shade(std::uint32_t color, float factor) {
    const auto channel = [factor](std::uint32_t value) {
        return static_cast<std::uint32_t>(std::clamp(value * factor, 0.0F, 255.0F));
    };
    return (channel((color >> 16U) & 0xffU) << 16U) |
           (channel((color >> 8U) & 0xffU) << 8U) |
           channel(color & 0xffU);
}

constexpr std::array<const char*, 10> kWorldNames{
    "RED HEAT", "ASTEROID BELT", "INTO THE SUN", "CRAB NEBULA", "BLUE PLANET",
    "OVER THE BASE", "SATELLITE", "THE EARTH", "MISTY", "DRUIDIA"};

std::uint32_t palette_color(const Road& road, std::size_t index, std::uint32_t fallback) {
    return index < road.palette.size() ? rgb(road.palette[index]) : fallback;
}

} // namespace

Canvas::Canvas() : pixels_(static_cast<std::size_t>(kScreenWidth * kScreenHeight)) {}

void Canvas::clear(std::uint32_t color) {
    std::fill(pixels_.begin(), pixels_.end(), color);
}

void Canvas::pixel(int x, int y, std::uint32_t color) {
    if (x >= 0 && x < kScreenWidth && y >= 0 && y < kScreenHeight) {
        pixels_[static_cast<std::size_t>(y * kScreenWidth + x)] = color;
    }
}

void Canvas::rectangle(int x, int y, int width, int height, std::uint32_t color) {
    const int x0 = std::clamp(x, 0, kScreenWidth);
    const int y0 = std::clamp(y, 0, kScreenHeight);
    const int x1 = std::clamp(x + width, 0, kScreenWidth);
    const int y1 = std::clamp(y + height, 0, kScreenHeight);
    for (int row = y0; row < y1; ++row) {
        std::fill(pixels_.begin() + row * kScreenWidth + x0,
                  pixels_.begin() + row * kScreenWidth + x1, color);
    }
}

void Canvas::line(int x0, int y0, int x1, int y1, std::uint32_t color) {
    const int dx = std::abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    while (true) {
        pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        const int doubled = error * 2;
        if (doubled >= dy) { error += dy; x0 += sx; }
        if (doubled <= dx) { error += dx; y0 += sy; }
    }
}

void Canvas::trapezoid(
    float far_left,
    float far_right,
    float far_y,
    float near_left,
    float near_right,
    float near_y,
    std::uint32_t color) {
    if (near_y < far_y) {
        std::swap(far_y, near_y);
        std::swap(far_left, near_left);
        std::swap(far_right, near_right);
    }
    const int start = std::max(0, static_cast<int>(std::floor(far_y)));
    const int end = std::min(kScreenHeight - 1, static_cast<int>(std::ceil(near_y)));
    const float span = std::max(near_y - far_y, 0.001F);
    for (int y = start; y <= end; ++y) {
        const float t = std::clamp((static_cast<float>(y) - far_y) / span, 0.0F, 1.0F);
        const int left = std::max(0, static_cast<int>(std::floor(far_left + (near_left - far_left) * t)));
        const int right = std::min(kScreenWidth - 1,
                                   static_cast<int>(std::ceil(far_right + (near_right - far_right) * t)));
        if (right >= left) {
            std::fill(pixels_.begin() + y * kScreenWidth + left,
                      pixels_.begin() + y * kScreenWidth + right + 1, color);
        }
    }
}

void Canvas::blit(
    const ImageFrame& frame,
    const ImagePalette& palette,
    int x,
    int y,
    bool transparent_zero) {
    for (int source_y = 0; source_y < frame.height; ++source_y) {
        for (int source_x = 0; source_x < frame.width; ++source_x) {
            const auto source_index = static_cast<std::size_t>(source_y * frame.width + source_x);
            if (source_index >= frame.pixels.size()) continue;
            const auto color_index = frame.pixels[source_index];
            if (transparent_zero && color_index == 0) continue;
            if (color_index < palette.colors.size()) {
                pixel(x + source_x, y + source_y, rgb(palette.colors[color_index]));
            }
        }
    }
}

void Canvas::blit_rotated_sprite(
    const ImageFrame& strip,
    const ImagePalette& palette,
    std::size_t sprite_index,
    int x,
    int y,
    bool rotate_clockwise) {
    constexpr int source_width = 24;
    constexpr int source_height = 22;
    const auto row_start = sprite_index * source_height;
    if (strip.width != source_width || row_start + source_height > strip.height) return;
    for (int sy = 0; sy < source_height; ++sy) {
        for (int sx = 0; sx < source_width; ++sx) {
            const auto source_index = static_cast<std::size_t>((row_start + sy) * source_width + sx);
            const auto color_index = strip.pixels[source_index];
            if (color_index == 0 || color_index >= palette.colors.size()) continue;
            const int dx = rotate_clockwise ? source_height - 1 - sy : sx;
            const int dy = rotate_clockwise ? sx : sy;
            pixel(x + dx, y + dy, rgb(palette.colors[color_index]));
        }
    }
}

void Canvas::text(int x, int y, const std::string& value, std::uint32_t color, int scale) {
    int cursor = x;
    for (char c : value) {
        if (c == ' ') {
            cursor += 6 * scale;
            continue;
        }
        const auto rows = glyph(c);
        for (int gy = 0; gy < 7; ++gy) {
            for (int gx = 0; gx < 5; ++gx) {
                if ((rows[gy] & (1U << (4 - gx))) != 0) {
                    rectangle(cursor + gx * scale, y + gy * scale, scale, scale, color);
                }
            }
        }
        cursor += 6 * scale;
    }
}

Game::Game(Assets assets) : assets_(std::move(assets)) {
    if (assets_.roads.roads.size() < 2) {
        throw DataError("SkyRoads needs the demo road and at least one playable road");
    }
}

void Game::start_level(std::size_t archive_index) {
    level_index_ = std::clamp<std::size_t>(archive_index, 1, assets_.roads.roads.size() - 1);
    reset_level();
    mode_ = GameMode::Playing;
    audio_cue_ = AudioCue::None;
}

void Game::reset_level() {
    const auto& road = assets_.roads.roads[level_index_];
    distance_ = 0;
    speed_ = 0;
    lane_ = 0;
    altitude_ = 0;
    vertical_velocity_ = 0;
    fuel_ = road.fuel;
    oxygen_ = road.oxygen;
    state_time_ = 0;
    airborne_ = false;
}

void Game::crash() {
    if (mode_ != GameMode::Playing) return;
    mode_ = GameMode::Crashed;
    state_time_ = 0;
    audio_cue_ = AudioCue::Crash;
}

std::uint16_t Game::current_cell() const {
    const auto& road = assets_.roads.roads[level_index_];
    if (road.rows.empty()) return 0;
    const auto row = std::min<std::size_t>(static_cast<std::size_t>(std::max(0.0, std::floor(distance_ + 0.6))),
                                           road.rows.size() - 1);
    const auto column = static_cast<std::size_t>(std::clamp(static_cast<int>(std::lround(lane_)) + 3, 0, 6));
    return road.rows[row][column];
}

void Game::update(const Input& input, double seconds) {
    ++ticks_;
    state_time_ += seconds;

    if (mode_ == GameMode::Title) {
        if (input.back_pressed) {
            quit_requested_ = true;
            return;
        }
        if (input.previous_level_pressed) {
            level_index_ = level_index_ <= 1 ? assets_.roads.roads.size() - 1 : level_index_ - 1;
        }
        if (input.next_level_pressed) {
            level_index_ = level_index_ + 1 >= assets_.roads.roads.size() ? 1 : level_index_ + 1;
        }
        if (input.confirm_pressed || input.jump_pressed) {
            start_level(level_index_);
        }
        return;
    }

    if (input.back_pressed) {
        mode_ = GameMode::Title;
        state_time_ = 0;
        audio_cue_ = AudioCue::Intro;
        return;
    }
    if ((mode_ == GameMode::Crashed || mode_ == GameMode::Complete) &&
        (input.restart_pressed || input.confirm_pressed || input.jump_pressed) && state_time_ > 0.25) {
        start_level(level_index_);
        return;
    }
    if (mode_ != GameMode::Playing) {
        speed_ = std::max(0.0, speed_ - seconds * 9.0);
        return;
    }

    const auto& road = assets_.roads.roads[level_index_];
    const double acceleration = input.accelerate ? 8.0 : -1.4;
    speed_ = std::clamp(speed_ + acceleration * seconds, 0.0, 12.0);
    if (input.brake) speed_ = std::max(0.0, speed_ - 12.0 * seconds);

    const double steering = (input.right ? 1.0 : 0.0) - (input.left ? 1.0 : 0.0);
    lane_ = std::clamp(lane_ + steering * seconds * (2.8 + speed_ * 0.08), -3.25, 3.25);

    const double advance = speed_ * seconds;
    distance_ += advance;
    fuel_ = std::max(0.0, fuel_ - advance);
    oxygen_ = std::max(0.0, oxygen_ - seconds);

    const auto cell = current_cell();
    const bool surface = (cell & 0x000fU) != 0;
    const bool obstacle = (cell & 0x6000U) != 0;
    const double gravity = std::max(4.0, static_cast<double>(road.gravity)) * 0.78;

    if (!airborne_) {
        if (input.jump_pressed && surface) {
            airborne_ = true;
            altitude_ = 0.03;
            vertical_velocity_ = 5.4;
            audio_cue_ = AudioCue::Jump;
        } else if (!surface) {
            airborne_ = true;
            vertical_velocity_ = 0;
        } else if (obstacle) {
            crash();
        }
    } else {
        vertical_velocity_ -= gravity * seconds;
        altitude_ += vertical_velocity_ * seconds;
        if (surface && altitude_ <= 0 && vertical_velocity_ <= 0) {
            if (obstacle) {
                crash();
            } else {
                altitude_ = 0;
                vertical_velocity_ = 0;
                airborne_ = false;
            }
        } else if (altitude_ < -3.5) {
            crash();
        }
    }

    if (fuel_ <= 0 || oxygen_ <= 0) {
        crash();
    } else if (distance_ >= static_cast<double>(road.rows.size())) {
        mode_ = GameMode::Complete;
        state_time_ = 0;
        audio_cue_ = AudioCue::Complete;
    }
}

void Game::render(Canvas& canvas) const {
    if (mode_ == GameMode::Title) render_title(canvas);
    else render_game(canvas);
}

void Game::render_title(Canvas& canvas) const {
    canvas.clear(0x000000);
    if (!assets_.intro.fragments.empty()) {
        const auto& background = assets_.intro.fragments[0];
        canvas.blit(background, assets_.intro.palettes[background.palette_index], 0, 0, false);
    }
    if (assets_.intro.fragments.size() > 1) {
        const auto& logo = assets_.intro.fragments[1];
        canvas.blit(logo, assets_.intro.palettes[logo.palette_index], logo.x_offset(), logo.y_offset(), true);
    }
    if (!assets_.main_menu.fragments.empty()) {
        const auto& menu = assets_.main_menu.fragments[0];
        canvas.blit(menu, assets_.main_menu.palettes[menu.palette_index], menu.x_offset(), menu.y_offset(), true);
    }

    const auto world = (level_index_ - 1) / 3;
    const auto road_in_world = (level_index_ - 1) % 3 + 1;
    char line[80]{};
    std::snprintf(line, sizeof(line), "[%02u] %s ROAD %u",
                  static_cast<unsigned>(level_index_), kWorldNames[world], static_cast<unsigned>(road_in_world));
    canvas.rectangle(0, 181, 320, 19, 0x000000);
    canvas.text(4, 183, line, 0x71d8ff);
    canvas.text(4, 192, "[ ] SELECT   ENTER START", 0xffffff);
    canvas.text(5, 5, "NATIVE WINDOWS PORT MILESTONE 1", 0xa9c8ff);
}

void Game::render_road(Canvas& canvas, const Road& road) const {
    if (road.rows.empty()) return;
    const auto base_row = static_cast<std::size_t>(std::max(0.0, std::floor(distance_)));
    const float fraction = static_cast<float>(distance_ - std::floor(distance_));
    constexpr int lookahead = 42;
    constexpr float center = 160.0F;

    const auto project_y = [](float depth) { return 38.0F + 108.0F / (depth * 0.32F + 0.78F); };
    const auto lane_width = [](float depth) { return 56.0F / (depth * 0.24F + 0.92F); };

    for (int ahead = lookahead; ahead >= 0; --ahead) {
        const auto row_index = base_row + static_cast<std::size_t>(ahead);
        if (row_index >= road.rows.size()) continue;
        const float near_depth = std::max(0.25F, static_cast<float>(ahead) + 0.7F - fraction);
        const float far_depth = near_depth + 1.0F;
        const float near_y = project_y(near_depth);
        const float far_y = project_y(far_depth);
        const float near_width = lane_width(near_depth);
        const float far_width = lane_width(far_depth);

        for (int column = 0; column < 7; ++column) {
            const auto descriptor = road.rows[row_index][static_cast<std::size_t>(column)];
            const auto bottom = static_cast<std::size_t>(descriptor & 0x000fU);
            const auto top = static_cast<std::size_t>((descriptor >> 4U) & 0x000fU);
            const bool tunnel = (descriptor & 0x1000U) != 0;
            const bool half = (descriptor & 0x2000U) != 0;
            const bool full = (descriptor & 0x4000U) != 0;
            const float lane0 = static_cast<float>(column) - 3.5F;
            const float far_left = center + lane0 * far_width;
            const float far_right = far_left + far_width;
            const float near_left = center + lane0 * near_width;
            const float near_right = near_left + near_width;

            if (bottom != 0) {
                const auto surface = palette_color(road, bottom, 0x707070);
                canvas.trapezoid(far_left, far_right, far_y, near_left, near_right, near_y, surface);
                canvas.line(static_cast<int>(near_left), static_cast<int>(near_y),
                            static_cast<int>(near_right), static_cast<int>(near_y), shade(surface, 1.16F));
            }

            if (half || full) {
                const float height = (full ? 72.0F : 39.0F) / (near_depth * 0.25F + 0.9F);
                const auto front = palette_color(road, std::min<std::size_t>(16 + bottom, 71), 0x8a4353);
                const auto top_color = top != 0 ? palette_color(road, top, 0xd0d0d0)
                                                : palette_color(road, 61, 0xd0d0d0);
                canvas.trapezoid(far_left, far_right, far_y - height * 0.75F,
                                 near_left, near_right, near_y - height, top_color);
                canvas.trapezoid(near_left, near_right, near_y - height,
                                 near_left, near_right, near_y, front);
            }

            if (tunnel) {
                const float height = 94.0F / (near_depth * 0.24F + 0.85F);
                const auto wall = palette_color(road, 69, 0x598b8c);
                const int thickness = std::max(1, static_cast<int>(near_width * 0.12F));
                canvas.rectangle(static_cast<int>(near_left), static_cast<int>(near_y - height),
                                 thickness, static_cast<int>(height), wall);
                canvas.rectangle(static_cast<int>(near_right) - thickness, static_cast<int>(near_y - height),
                                 thickness, static_cast<int>(height), wall);
                canvas.rectangle(static_cast<int>(near_left), static_cast<int>(near_y - height),
                                 std::max(1, static_cast<int>(near_right - near_left)), thickness, shade(wall, 1.18F));
            }
        }
    }
}

void Game::render_ship(Canvas& canvas) const {
    if (assets_.cars.fragments.empty() || assets_.cars.palettes.empty()) return;
    const auto& strip = assets_.cars.fragments[0];
    const auto& palette = assets_.cars.palettes[strip.palette_index];
    const int ship_x = static_cast<int>(160 + lane_ * 27.0) - 11;
    const int ship_y = static_cast<int>(105 - altitude_ * 9.0);

    canvas.rectangle(ship_x + 6, 124, 11, 2, 0x101018);
    if (mode_ == GameMode::Crashed) {
        const auto explosion = std::min<std::size_t>(6, static_cast<std::size_t>(state_time_ * 10.0));
        canvas.blit_rotated_sprite(strip, palette, explosion, ship_x, ship_y, false);
        return;
    }

    const bool jumping = airborne_ && altitude_ > 0;
    std::size_t group = 27;
    if (lane_ < -0.12) group = jumping ? 36 : 21;
    else if (lane_ > 0.12) group = jumping ? 45 : 30;
    else group = jumping ? 42 : 27;
    const std::size_t phase = speed_ > 0.2 ? (ticks_ / 5) % 3 : 0;
    canvas.blit_rotated_sprite(strip, palette, group + phase, ship_x, ship_y, true);
}

void Game::render_game(Canvas& canvas) const {
    canvas.clear(0x000000);
    const auto world_index = std::min<std::size_t>((level_index_ - 1) / 3, assets_.worlds.size() - 1);
    const auto& world = assets_.worlds[world_index];
    if (!world.fragments.empty()) {
        const auto& image = world.fragments[0];
        canvas.blit(image, world.palettes[image.palette_index], 0, 0, false);
    }
    const auto& road = assets_.roads.roads[level_index_];
    render_road(canvas, road);
    render_ship(canvas);

    if (!assets_.dashboard.fragments.empty()) {
        const auto& dashboard = assets_.dashboard.fragments[0];
        canvas.blit(dashboard, assets_.dashboard.palettes[dashboard.palette_index], 0, 129, false);
    }

    char status[96]{};
    const auto road_in_world = (level_index_ - 1) % 3 + 1;
    std::snprintf(status, sizeof(status), "%s %u  SPD %02u  F %03u  O %03u",
                  kWorldNames[world_index], static_cast<unsigned>(road_in_world),
                  static_cast<unsigned>(std::lround(speed_)),
                  static_cast<unsigned>(std::max(0.0, fuel_)),
                  static_cast<unsigned>(std::max(0.0, oxygen_)));
    canvas.rectangle(0, 0, 320, 10, 0x000000);
    canvas.text(3, 2, status, 0xffffff);

    if (mode_ == GameMode::Crashed) {
        canvas.rectangle(84, 74, 152, 29, 0x000000);
        canvas.text(105, 79, "ROAD FAILED", 0xff684f, 2);
        canvas.text(91, 96, "SPACE OR ENTER RETRY", 0xffffff);
    } else if (mode_ == GameMode::Complete) {
        canvas.rectangle(76, 74, 168, 29, 0x000000);
        canvas.text(91, 79, "ROAD COMPLETE", 0x77ff91, 2);
        canvas.text(91, 96, "SPACE OR ENTER RETRY", 0xffffff);
    }
}

AudioCue Game::consume_audio_cue() {
    const auto cue = audio_cue_;
    audio_cue_ = AudioCue::None;
    return cue;
}

} // namespace skyroads
