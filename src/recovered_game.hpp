#pragma once

#include "hd_renderer.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

namespace skyroads {

inline constexpr int kScreenWidth = 320;
inline constexpr int kScreenHeight = 200;
inline constexpr std::uint32_t kDosPitClock = 1193182u;
inline constexpr std::uint32_t kDosPitDivisor = 0x19e4u;
inline constexpr double kDosIrqRate =
    static_cast<double>(kDosPitClock) / static_cast<double>(kDosPitDivisor);

struct NativeInput {
    bool left{};
    bool right{};
    bool up{};
    bool down{};
    bool jump{};
    bool gamepad_active{};
    bool enter_pressed{};
    bool escape_pressed{};
    bool joystick_connected{};
    std::uint16_t joystick_x{0x8000u};
    std::uint16_t joystick_y{0x8000u};
    bool joystick_button{};
    bool mouse_available{};
    std::uint16_t mouse_x{0x00a0u};
    std::uint16_t mouse_y{0x0064u};
    bool mouse_button{};
    bool editor_space_pressed{};
    bool editor_shape_pressed{};
    bool editor_save_pressed{};
    bool editor_play_pressed{};
    bool editor_theme_pressed{};
    bool editor_gravity_pressed{};
    bool editor_fuel_pressed{};
    bool editor_oxygen_pressed{};
    bool editor_insert_pressed{};
    bool editor_delete_pressed{};
    bool editor_page_up_pressed{};
    bool editor_page_down_pressed{};
    bool editor_mouse_pressed{};
    bool editor_view_pressed{};
    bool kosmonaut_demo_pressed{};
    bool backspace_pressed{};
    std::uint8_t text_character{};
    std::int8_t editor_material_shortcut{-1};
    bool cheat_air_jump_pressed{};
    bool cheat_refill_pressed{};
    bool cheat_no_gravity_pressed{};
    bool cheat_overdrive_pressed{};
};

enum class NativeScreen {
    Intro,
    MainMenu,
    Settings,
    Options,
    Help,
    LevelSelection,
    CustomLevelBrowser,
    CustomLevelEditor,
    LevelTransition,
    Playing,
    Demo,
    LevelResult,
    Kosmonaut
};

enum class NativeAspectRatio : std::uint8_t {
    Original,
    Widescreen,
    UltraWidescreen
};

struct PcmEffect {
    unsigned effect{};
    std::uint32_t sample_rate{};
    std::vector<std::uint8_t> samples;
    bool loop{};
};

struct OplRegisterWrite {
    std::uint8_t reg{};
    std::uint8_t value{};
};

class RecoveredGame final {
public:
    explicit RecoveredGame(const std::filesystem::path& data_root);
    ~RecoveredGame();
    RecoveredGame(RecoveredGame&&) noexcept;
    RecoveredGame& operator=(RecoveredGame&&) noexcept;
    RecoveredGame(const RecoveredGame&) = delete;
    RecoveredGame& operator=(const RecoveredGame&) = delete;

    /* One invocation of the original 180 Hz INT 08h schedule. */
    void timer_tick(const NativeInput& input);

    [[nodiscard]] const std::vector<std::uint32_t>& pixels() const;
    [[nodiscard]] const std::vector<std::uint8_t>& indexed_pixels() const;
    [[nodiscard]] NativeScreen screen() const;
    [[nodiscard]] std::size_t level_index() const;
    [[nodiscard]] std::size_t level_count() const;
    [[nodiscard]] bool has_xmas_levels() const;
    [[nodiscard]] std::size_t custom_level_count() const;
    [[nodiscard]] std::uint32_t road_distance() const;
    [[nodiscard]] std::uint16_t gameplay_ticks() const;
    [[nodiscard]] std::uint16_t last_ship_frame() const;
    [[nodiscard]] std::uint16_t selected_input_mode() const;
    [[nodiscard]] bool high_definition_enabled() const;
    [[nodiscard]] NativeAspectRatio aspect_ratio_mode() const;
    [[nodiscard]] bool quit_requested() const;
    [[nodiscard]] bool high_definition_scene_available() const;
    [[nodiscard]] const std::vector<std::uint32_t>&
        high_definition_background_pixels() const;
    [[nodiscard]] const std::vector<std::uint32_t>&
        high_definition_road_pixels() const;
    [[nodiscard]] const std::vector<RecoveredRoadShape>&
        high_definition_road_shapes() const;
    [[nodiscard]] std::size_t high_definition_ship_layer() const;
    [[nodiscard]] const WideRoadScene& wide_road_scene() const;
    [[nodiscard]] const RecoveredShipModel& high_definition_ship_model() const;
    [[nodiscard]] const std::vector<std::uint8_t>&
        high_definition_ship_exclusion_mask() const;
    [[nodiscard]] bool air_jump_enabled() const;
    [[nodiscard]] bool no_gravity_enabled() const;
    [[nodiscard]] bool overdrive_enabled() const;
    [[nodiscard]] std::uint16_t fuel() const;
    [[nodiscard]] std::uint16_t oxygen() const;
    [[nodiscard]] std::int16_t vertical_velocity() const;
    [[nodiscard]] std::int16_t lateral_velocity() const;
    [[nodiscard]] std::int16_t gravity_step() const;
    [[nodiscard]] std::int32_t forward_speed() const;
    [[nodiscard]] std::int32_t forward_speed_limit() const;
    /* Increments only when the recovered DOS code presents a completed frame. */
    [[nodiscard]] std::uint64_t presentation_revision() const;
    [[nodiscard]] const std::filesystem::path& data_root() const;

    /* Oracle diagnostic: current mutable expanded TREKDAT record bytes. */
    [[nodiscard]] std::vector<std::uint8_t> trek_record_bytes(std::size_t index) const;
    [[nodiscard]] std::vector<std::uint8_t> car_frame_bytes(std::size_t index) const;
    [[nodiscard]] std::vector<std::uint8_t> ship_mask_bytes() const;
    /* Opt-in executable-oracle diagnostic; disabled during normal play. */
    void begin_palette_trace();
    [[nodiscard]] std::vector<std::uint8_t> consume_palette_trace();

    /* Exact offset-delimited sfx.snd payload selected by original dispatch. */
    std::optional<PcmEffect> consume_pcm_effect();

    /* Register writes emitted by the executable-recovered OPL scheduler. */
    std::vector<OplRegisterWrite> consume_opl_writes();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace skyroads
