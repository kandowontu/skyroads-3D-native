#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace skyroads {

struct NativeInput;

struct KosmonautSound {
    unsigned effect{};
    std::uint32_t sample_rate{};
    std::vector<std::uint8_t> samples;
    bool loop{};
};

class KosmonautGame final {
public:
    explicit KosmonautGame(std::filesystem::path save_directory);

    void enter();
    /* Runs one recovered game step. The host schedules these at roughly
       18 Hz, matching the original EGA renderer on a 386-class machine. */
    bool tick(const NativeInput& input);

    [[nodiscard]] const std::vector<std::uint8_t>& pixels() const;
    [[nodiscard]] const std::array<std::uint8_t, 256u * 3u>& palette() const;
    [[nodiscard]] std::optional<KosmonautSound> consume_sound();
    [[nodiscard]] unsigned selected_road() const;
    [[nodiscard]] unsigned unlocked_roads() const;
    [[nodiscard]] bool playing() const;

private:
    enum class Phase {
        Title,
        TitleExit,
        Tutorial,
        RoadSelect,
        Playing,
        Paused,
        RoadComplete,
        AllComplete,
        Crashed,
        Demo,
        NameEntry
    };

    enum class CrashCause {
        Fall,
        Collision,
        Fuel,
        Oxygen,
        Slippery,
        Explosion,
        OffRoad
    };

    struct HighScore {
        std::string name;
        std::uint16_t score{};
        std::uint8_t road{};
    };

    struct StarState {
        std::uint16_t path_offset{0x0700u};
        std::int32_t screen_offset{0x071b};
        std::uint8_t spread{};
        std::uint8_t mask{1u};
    };

    std::filesystem::path save_path_;
    std::vector<std::uint8_t> pixels_;
    std::array<std::uint8_t, 256u * 3u> palette_{};
    Phase phase_{Phase::Title};
    unsigned selected_road_{};
    unsigned unlocked_roads_{2u};
    unsigned phase_ticks_{};
    unsigned animation_tick_{};
    std::uint32_t score_{};
    std::uint32_t road_score_base_{};
    std::uint32_t session_best_score_{};
    std::uint32_t best_score_{};
    std::array<HighScore, 9> high_scores_{};
    unsigned name_entry_slot_{9u};
    std::int32_t forward_position_{4};
    std::int32_t forward_fraction_{};
    std::int32_t forward_speed_{};
    std::int32_t lateral_position_{0xc0};
    std::int32_t lateral_velocity_{};
    std::int32_t altitude_{9};
    std::int32_t vertical_velocity_{};
    std::int32_t fuel_{32000};
    std::int32_t oxygen_{32000};
    std::int32_t jump_power_{0x78};
    std::int32_t saved_jump_power_{0x78};
    std::int32_t previous_forward_position_{4};
    std::int32_t previous_lateral_position_{0xc0};
    std::int32_t previous_altitude_{9};
    std::int32_t lateral_step_{};
    std::int32_t speed_penalty_{};
    std::int32_t bounce_height_{};
    std::int32_t bounce_velocity_{};
    unsigned ship_animation_{};
    unsigned tutorial_pass_{};
    unsigned session_start_road_{};
    unsigned idle_ticks_{};
    unsigned death_ticks_{};
    bool checkpoint_reached_{};
    bool grounded_{true};
    bool slippery_{};
    bool speed_penalty_active_{};
    bool dying_{};
    bool burning_{};
    bool demo_session_{};
    bool session_had_input_{};
    bool sound_enabled_{true};
    CrashCause crash_cause_{CrashCause::OffRoad};
    std::uint8_t surface_tile_{};
    std::uint8_t left_tile_{};
    std::uint8_t right_tile_{};
    std::uint8_t last_effect_tile_{};
    std::optional<KosmonautSound> pending_sound_;
    std::array<StarState, 120> stars_{};
    std::array<std::uint8_t, 16> star_rng_state_{};
    std::uint8_t star_rng_value_{};
    std::uint8_t star_frame_{};
    std::int32_t star_previous_forward_{-1};

    void load_save();
    void save_progress() const;
    void reset_road(bool demo);
    void render();
    void render_title();
    void render_road_select();
    void render_playfield();
    void render_status(const char* text, std::uint8_t color = 0x0fu);
    void render_ship(const std::vector<std::uint8_t>* occlusion = nullptr);
    void render_original_hud();
    void render_high_scores();
    void finish_session();
    void update_playing(const NativeInput& input, bool demo);
    int apply_tile_effect(std::uint8_t tile);
    void begin_complete();
    void begin_crash(CrashCause cause = CrashCause::Collision);
    void sound_tone(unsigned effect, unsigned frequency, unsigned milliseconds);
    void schedule_music(unsigned track);
    void silence_sound();
    void reset_starfield();
    void advance_starfield();
    void draw_starfield();
    [[nodiscard]] std::uint8_t next_star_random();
    [[nodiscard]] std::uint8_t current_tile() const;
    [[nodiscard]] std::uint8_t road_tile(unsigned row, unsigned lane) const;
    [[nodiscard]] unsigned current_row() const;
    [[nodiscard]] bool test_collision(
        std::int32_t forward, std::int32_t lateral,
        std::int32_t altitude, std::int32_t old_altitude);
    [[nodiscard]] bool predicts_safe_landing(
        std::int32_t speed, std::int32_t lateral_step) const;
    void find_landing_adjustment();
    [[nodiscard]] const char* crash_message() const;
    [[nodiscard]] int crash_message_x() const;
};

} // namespace skyroads
