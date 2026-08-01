#include "recovered_game.hpp"
#include "custom_levels.hpp"
#include "editor_3d_view.hpp"
#include "embedded_game_data.hpp"
#include "expanded_level_menu.hpp"

extern "C" {
#include "car_sprites.h"
#include "config.h"
#include "dashboard.h"
#include "display_table.h"
#include "embedded_hud.h"
#include "gameplay.h"
#include "graphics_archive.h"
#include "animation_archive.h"
#include "bios_font.h"
#include "input.h"
#include "intro.h"
#include "level_flow.h"
#include "menu_flow.h"
#include "menus.h"
#include "music.h"
#include "palette.h"
#include "physics.h"
#include "picture_blitter.h"
#include "render_params.h"
#include "renderer_tables.h"
#include "renderer_vga.h"
#include "road_archive.h"
#include "sound_effects.h"
#include "trek_archive.h"
}

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace skyroads {
namespace {

constexpr std::size_t kFramebufferSize =
    static_cast<std::size_t>(kScreenWidth) * kScreenHeight;
constexpr std::size_t kPaletteSize = 256u * 3u;
constexpr std::array<std::uint16_t, 10> kEditorMaterials{
    0u, 1u, 2u, 3u, 5u, 8u, 9u, 10u, 12u, 14u};
constexpr std::int32_t kOriginalForwardSpeedLimit = 0x2aaa;
constexpr std::int32_t kOverdriveForwardSpeedLimit = 0x5554;
constexpr unsigned kNativeSettingsItemCount = 6u;

std::string fixed_number(std::size_t value, unsigned digits) {
    auto text = std::to_string(value);
    if (text.size() < digits) text.insert(text.begin(), digits - text.size(), '0');
    return text;
}

std::string material_tool_name(unsigned slot) {
    constexpr std::array<const char*, 10> names{
        "ERASE", "ROAD", "SLOW", "BLUE ROAD", "GOLD ROAD",
        "SLIDE", "REFILL", "BOOST", "TERMINAL", "WHITE ROAD"};
    return slot < names.size() ? names[slot] : "ROAD";
}

std::string shape_name(unsigned shape) {
    constexpr std::array<const char*, 6> names{
        "FLAT", "ARCH", "WALL", "RAMP", "HIGH", "HIGH RAMP"};
    return shape < names.size() ? names[shape] : "UNKNOWN";
}

std::string lower_ascii(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return text;
}

std::filesystem::path find_file(
    const std::filesystem::path& root,
    const std::string& name) {
    const auto exact = root / name;
    if (std::filesystem::exists(exact)) return exact;
    const auto wanted = lower_ascii(name);
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(root, error)) {
        if (entry.is_regular_file() &&
            lower_ascii(entry.path().filename().string()) == wanted) {
            return entry.path();
        }
    }
    throw std::runtime_error("Missing original SkyRoads file: " + name);
}

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) throw std::runtime_error("Unable to open " + path.string());
    const auto end = stream.tellg();
    if (end < 0) throw std::runtime_error("Unable to size " + path.string());
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
    stream.seekg(0);
    if (!bytes.empty() &&
        !stream.read(reinterpret_cast<char*>(bytes.data()), end)) {
        throw std::runtime_error("Unable to read " + path.string());
    }
    return bytes;
}

std::filesystem::path find_original_executable(
    const std::filesystem::path& root) {
    for (const auto* name : {"skyroads.exe", "skyxmas.exe"}) {
        try {
            return find_file(root, name);
        }
        catch (const std::exception&) {
        }
    }
    throw std::runtime_error(
        "The game folder must contain an original SKYROADS.EXE or SKYXMAS.EXE");
}

std::vector<std::uint8_t> read_embedded_file(
    EmbeddedCampaign campaign,
    const std::string& name) {
    const auto bytes = embedded_game_file(campaign, lower_ascii(name));
    if (bytes.empty()) {
        throw std::runtime_error("Missing embedded original SkyRoads file: " + name);
    }
    return {bytes.begin(), bytes.end()};
}

void copy_palette(
    std::array<std::uint8_t, kPaletteSize>& destination,
    unsigned base,
    const std::uint8_t* source,
    unsigned count) {
    if (base + count > 256u) {
        throw std::runtime_error("Original palette section exceeds the VGA DAC");
    }
    std::memcpy(destination.data() + base * 3u, source, count * 3u);
}

void draw_picture(
    std::vector<std::uint8_t>& destination,
    const SrPicture& picture,
    std::optional<std::uint16_t> screen_offset = std::nullopt) {
    const std::size_t start = screen_offset.value_or(picture.screen_offset);
    for (std::size_t y = 0; y < picture.height; ++y) {
        const std::size_t row = start + y * kScreenWidth;
        if (row > destination.size() || picture.width > destination.size() - row) {
            throw std::runtime_error("Original menu picture falls outside 320x200 VGA memory");
        }
        for (std::size_t x = 0; x < picture.width; ++x) {
            const auto pixel = picture.pixels[y * picture.width + x];
            if (pixel != 0) destination[row + x] = pixel;
        }
    }
}

void draw_main_menu_editor_label(
    std::vector<std::uint8_t>& destination,
    bool selected) {
    struct MenuGlyph {
        unsigned width;
        std::array<std::uint16_t, 16> rows;
    };
    static constexpr std::array<MenuGlyph, 6> glyphs{{
        {9u, {0x1ff,0x1ff,0x1ff,0x1c0,0x1c0,0x1c0,0x1fc,0x1fc,
              0x1fc,0x1c0,0x1c0,0x1c0,0x1ff,0x1ff,0x1ff,0x1ff}},
        {9u, {0x007,0x007,0x007,0x007,0x07f,0x0ff,0x1c7,0x1c7,
              0x1c7,0x1c7,0x1c7,0x1c7,0x1c7,0x1c7,0x0ff,0x07f}},
        {3u, {0x007,0x007,0x007,0x000,0x000,0x007,0x007,0x007,
              0x007,0x007,0x007,0x007,0x007,0x007,0x007,0x007}},
        {7u, {0x01c,0x01c,0x01c,0x01c,0x07f,0x07f,0x07f,0x01c,
              0x01c,0x01c,0x01c,0x01c,0x01c,0x01f,0x00e,0x00c}},
        {9u, {0x000,0x000,0x000,0x07c,0x0fe,0x1c7,0x1c7,0x1c7,
              0x1c7,0x1c7,0x1c7,0x1c7,0x1c7,0x1c7,0x0fe,0x07c}},
        {8u, {0x000,0x000,0x000,0x0ee,0x0ff,0x0ff,0x0f7,0x0e7,
              0x0e7,0x0e0,0x0e0,0x0e0,0x0e0,0x0e0,0x0e0,0x0e0}},
    }};
    constexpr int origin_x = 132;
    constexpr int origin_y = 183;

    const auto draw_pass = [&](std::uint8_t color, bool outline) {
        int glyph_x = origin_x;
        for (const auto& glyph : glyphs) {
            for (unsigned row = 0; row < glyph.rows.size(); ++row) {
                for (unsigned column = 0; column < glyph.width; ++column) {
                    if ((glyph.rows[row] &
                            (1u << (glyph.width - 1u - column))) == 0u) continue;
                    const int radius = outline ? 1 : 0;
                    for (int offset_y = -radius; offset_y <= radius; ++offset_y) {
                        for (int offset_x = -radius; offset_x <= radius; ++offset_x) {
                            const int x = glyph_x + static_cast<int>(column) + offset_x;
                            const int y = origin_y + static_cast<int>(row) + offset_y;
                            if (x >= 0 && x < kScreenWidth &&
                                y >= 0 && y < kScreenHeight) {
                                destination[static_cast<std::size_t>(y) *
                                    kScreenWidth + static_cast<unsigned>(x)] = color;
                            }
                        }
                    }
                }
            }
            glyph_x += static_cast<int>(glyph.width) + 2;
        }
    };

    /* MAINMENU.LZS uses heavy white bodies with a one-pixel yellow halo only
       around the active word.  These title-case glyphs follow the original
       16-pixel menu proportions instead of borrowing the serifed BIOS font. */
    if (selected) draw_pass(0xbfu, true);
    draw_pass(0xc0u, false);
}

struct PictureOwner {
    SrPicture value{};
    PictureOwner() = default;
    ~PictureOwner() { sr_free_picture(&value); }
    PictureOwner(PictureOwner&& other) noexcept : value(other.value) {
        std::memset(&other.value, 0, sizeof(other.value));
    }
    PictureOwner& operator=(PictureOwner&& other) noexcept {
        if (this != &other) {
            sr_free_picture(&value);
            value = other.value;
            std::memset(&other.value, 0, sizeof(other.value));
        }
        return *this;
    }
    PictureOwner(const PictureOwner&) = delete;
    PictureOwner& operator=(const PictureOwner&) = delete;
};

struct PaletteSection {
    unsigned base{};
    std::vector<std::uint8_t> colors;

    [[nodiscard]] unsigned count() const {
        return static_cast<unsigned>(colors.size() / 3u);
    }
};

void apply_palette_section(
    std::array<std::uint8_t, kPaletteSize>& palette,
    const PaletteSection& section) {
    copy_palette(palette, section.base, section.colors.data(), section.count());
}

void blend_palette_section(
    std::array<std::uint8_t, kPaletteSize>& palette,
    const PaletteSection& from,
    const PaletteSection& to,
    unsigned percent) {
    if (from.base != to.base || from.colors.size() != to.colors.size()) {
        throw std::runtime_error("Original intro palette sections do not match");
    }
    const auto offset = static_cast<std::size_t>(from.base) * 3u;
    if (!sr_blend_palette_bytes(
            palette.data() + offset,
            from.colors.data(), to.colors.data(),
            static_cast<unsigned>(from.colors.size()),
            static_cast<std::uint16_t>(percent))) {
        throw std::runtime_error("Could not reproduce the DOS palette transition");
    }
}

void blend_full_palette(
    std::array<std::uint8_t, kPaletteSize>& output,
    const std::array<std::uint8_t, kPaletteSize>& from,
    const std::array<std::uint8_t, kPaletteSize>& to,
    unsigned percent) {
    if (!sr_blend_palette_bytes(
            output.data(), from.data(), to.data(),
            static_cast<unsigned>(output.size()),
            static_cast<std::uint16_t>(percent))) {
        throw std::runtime_error("Could not reproduce the DOS palette transition");
    }
}

class AssetStream {
public:
    explicit AssetStream(std::vector<std::uint8_t> bytes)
        : bytes_(std::move(bytes)) {}

    void color_map(
        std::array<std::uint8_t, kPaletteSize>& palette,
        unsigned base) {
        apply_palette_section(palette, color_map_section(base));
    }

    PaletteSection color_map_section(unsigned base) {
        if (cursor_ + 5u > bytes_.size() ||
            std::memcmp(bytes_.data() + cursor_, "CMAP", 4) != 0) {
            throw std::runtime_error("Expected CMAP in an original menu stream");
        }
        const auto count = static_cast<unsigned>(bytes_[cursor_ + 4u]);
        const std::size_t bytes_needed = 5u + count * 5u;
        if (bytes_needed > bytes_.size() - cursor_) {
            throw std::runtime_error("Truncated CMAP in an original menu stream");
        }
        PaletteSection section;
        section.base = base;
        section.colors.assign(
            bytes_.begin() + static_cast<std::ptrdiff_t>(cursor_ + 5u),
            bytes_.begin() + static_cast<std::ptrdiff_t>(
                cursor_ + 5u + count * 3u));
        cursor_ += bytes_needed;
        return section;
    }

    PictureOwner picture(unsigned palette_base) {
        PictureOwner picture;
        std::size_t consumed = 0;
        const auto error = sr_load_vga_picture(
            bytes_.data() + cursor_, bytes_.size() - cursor_,
            static_cast<std::uint8_t>(palette_base), &picture.value, &consumed);
        if (error != SR_GRAPHICS_ARCHIVE_OK) {
            throw std::runtime_error("Invalid PICT in an original menu stream");
        }
        cursor_ += consumed;
        return picture;
    }

private:
    std::vector<std::uint8_t> bytes_;
    std::size_t cursor_{};
};

struct MainMenuAssets {
    std::array<std::uint8_t, kPaletteSize> palette{};
    std::vector<std::uint8_t> background =
        std::vector<std::uint8_t>(kFramebufferSize);
    SrGraphicsArchive items{};
    std::vector<std::uint8_t> neutral_items;
    ~MainMenuAssets() { sr_free_graphics_archive(&items); }
};

struct SettingsAssets {
    std::array<std::uint8_t, kPaletteSize> palette{};
    std::vector<std::uint8_t> background =
        std::vector<std::uint8_t>(kFramebufferSize);
    std::vector<PictureOwner> pictures;
};

struct LevelMenuAssets {
    std::array<std::uint8_t, kPaletteSize> palette{};
    std::array<std::uint8_t, kPaletteSize> expanded_palette{};
    std::vector<std::uint8_t> background =
        std::vector<std::uint8_t>(kFramebufferSize);
    std::vector<std::uint8_t> xmas_background =
        std::vector<std::uint8_t>(kFramebufferSize);
    unsigned base_color_count{};
    PictureOwner completion_marker;
};

struct HelpPage {
    std::array<std::uint8_t, kPaletteSize> palette{};
    std::vector<std::uint8_t> framebuffer =
        std::vector<std::uint8_t>(kFramebufferSize);
};

struct IntroAssets {
    std::vector<std::uint8_t> background =
        std::vector<std::uint8_t>(kFramebufferSize);
    PaletteSection background_palette;
    PaletteSection logo_palette_from;
    PaletteSection logo_palette_to;
    PictureOwner logo;
    std::array<PaletteSection, 7> card_palette_from;
    std::array<PaletteSection, 7> card_palette_to;
    std::array<PictureOwner, 7> cards;
    SrAnimationArchive animation{};
    std::vector<std::uint8_t> sample;

    ~IntroAssets() { sr_free_animation_archive(&animation); }
};

std::uint16_t menu_key(const NativeInput& input) {
    if (input.enter_pressed) return SR_MENU_KEY_ENTER;
    if (input.escape_pressed) return SR_MENU_KEY_ESCAPE;
    if (input.up) return SR_MENU_KEY_UP;
    if (input.left) return SR_MENU_KEY_LEFT;
    if (input.right) return SR_MENU_KEY_RIGHT;
    if (input.down) return SR_MENU_KEY_DOWN;
    return 0;
}

} // namespace

struct RecoveredGame::Impl {
    explicit Impl(std::filesystem::path root_path)
        : root(std::filesystem::absolute(std::move(root_path))),
          indexed(kFramebufferSize), rgba(kFramebufferSize),
          gameplay_background(kFramebufferSize) {
        load_all();
        load_config_file();
        load_native_config_file();
        SrOplHooks opl_hooks{};
        opl_hooks.context = this;
        opl_hooks.write_register = native_opl_write;
        sr_opl_player_init(&opl, &opl_hooks);
        sr_sound_effect_state_init(&sound_state, 1);
        sr_main_menu_init(&main_state);
        start_intro();
    }

    ~Impl() {
        sr_free_sample_effect_archive(&samples);
        sr_free_music_archive(&music);
        sr_free_display_table(&speed_display);
        sr_free_display_table(&fuel_display);
        sr_free_display_table(&oxygen_display);
        for (auto& world : worlds) sr_free_graphics_archive(&world);
        for (auto& world : xmas_worlds) sr_free_graphics_archive(&world);
        sr_free_road_archive(&xmas_roads);
        sr_free_graphics_archive(&dashboard);
        sr_free_car_sprites(&cars);
        sr_free_trek_archive(&trek);
        sr_free_road_archive(&roads);
    }

    std::filesystem::path root;
    std::filesystem::path custom_level_directory;
    std::filesystem::path original_level_directory;
    std::vector<std::uint8_t> indexed;
    std::vector<std::uint32_t> rgba;
    std::uint64_t presentation_revision{};
    std::array<std::uint8_t, kPaletteSize> active_palette{};
    bool palette_trace_enabled{};
    std::array<std::uint8_t, kPaletteSize> palette_trace_previous{};
    std::vector<std::uint8_t> palette_trace;
    std::vector<std::uint8_t> gameplay_background;
    std::array<std::uint8_t, kPaletteSize> black_palette{};
    std::array<std::uint8_t, kPaletteSize> intro_animation_palette{};
    std::array<std::uint8_t, kPaletteSize> intro_final_palette{};
    std::array<std::uint8_t, kPaletteSize> gameplay_palette{};

    SrRoadArchive roads{};
    SrRoadArchive xmas_roads{};
    SrTrekArchive trek{};
    SrCarSprites cars{};
    SrRendererTables renderer_tables{};
    SrVgaRendererState renderer_state{};
    SrRoadFrameParams last_road_params{};
    SrEmbeddedHud hud{};
    SrGraphicsArchive dashboard{};
    std::array<SrGraphicsArchive, 10> worlds{};
    std::array<SrGraphicsArchive, 10> xmas_worlds{};
    bool xmas_available{};
    bool active_xmas_campaign{};
    bool active_custom_playtest{};
    SrRoadData custom_play_road{};
    std::vector<std::uint16_t> custom_play_cells;
    SrDisplayTable oxygen_display{};
    SrDisplayTable fuel_display{};
    SrDisplayTable speed_display{};
    SrMusicArchive music{};
    SrOplPlayer opl{};
    SrEmbeddedSoundEffects speaker_effects{};
    SrSampleEffectArchive samples{};
    SrSoundEffectState sound_state{};

    IntroAssets intro_assets;
    MainMenuAssets main_assets;
    SettingsAssets settings_assets;
    LevelMenuAssets level_assets;
    std::array<HelpPage, 2> help_pages;

    SrConfigBlock config_file{};
    SrConfigBlock xmas_config_file{};
    std::uint16_t selected_input_mode{};
    std::uint16_t sound_disabled{};
    std::array<std::uint16_t, kCombinedLevelCount> completion_count{};
    SrMainMenuState main_state{};
    unsigned main_selection{};
    SrSettingsMenuState settings_state{};
    unsigned settings_selection{};
    bool high_definition{};
    SrLevelMenuState level_state{};
    SrIntroSequence intro_sequence{};
    SrMenuFlowState menu_flow{};
    unsigned help_page{};
    NativeScreen active_screen{NativeScreen::Intro};

    std::vector<CustomLevel> custom_levels;
    bool custom_browser_original_levels{};
    std::size_t custom_browser_selection{};
    std::size_t custom_editor_index{};
    std::size_t custom_editor_row{};
    std::size_t custom_editor_column{3u};
    std::uint16_t custom_editor_brush_material{1u};
    std::uint16_t custom_editor_brush_shape{};
    EditorViewMode custom_editor_view{EditorViewMode::Top};
    bool custom_editor_dirty{};
    std::string custom_editor_status{"READY"};

    SrGameplayConfig gameplay_config{};
    SrGameplayState gameplay{};
    SrRunLevelState run_level_state{};
    NativeScreen run_level_play_screen{NativeScreen::Playing};
    SrLevelInput demo_input{};
    SrLevelInput live_input{};
    NativeInput latest_input{};
    bool air_jump_cheat{};
    bool no_gravity_cheat{};
    bool overdrive_cheat{};
    bool previous_jump_control{};
    std::string cheat_status;
    unsigned cheat_status_ticks{};
    std::vector<std::uint8_t> demo_record;
    SrDashboardState dashboard_state{};
    std::size_t road_index{};
    std::size_t active_road_record{1u};
    std::uint16_t tick_count{};
    bool finish_coast_active{};
    std::uint64_t irq_count{};
    std::uint8_t irq_phase{};
    std::uint16_t last_random_track{0xffffu};
    std::optional<unsigned> pending_effect;
    bool pending_intro_sample{};
    std::vector<OplRegisterWrite> pending_opl_writes;

    static void native_opl_write(void* context, std::uint8_t reg, std::uint8_t value) {
        static_cast<Impl*>(context)->pending_opl_writes.push_back({reg, value});
    }

    static void gameplay_sound(void* context, unsigned effect) {
        static_cast<Impl*>(context)->play_effect(effect);
    }

    static int gameplay_sound_active(void* context) {
        auto* self = static_cast<Impl*>(context);
        return sr_sound_effect_is_active(&self->sound_state, &self->speaker_effects);
    }

    static std::uint16_t native_joystick_axis(void* context, unsigned axis) {
        const auto* input = static_cast<const NativeInput*>(context);
        if (!input->joystick_connected) return 0;
        return axis == 1 ? input->joystick_x : input->joystick_y;
    }

    static std::uint16_t native_joystick_button(void* context) {
        const auto* input = static_cast<const NativeInput*>(context);
        return static_cast<std::uint16_t>(
            input->joystick_connected && input->joystick_button);
    }

    static std::uint16_t native_mouse_value(void* context, unsigned selector) {
        const auto* input = static_cast<const NativeInput*>(context);
        if (!input->mouse_available) return 0;
        if (selector == 0) return input->mouse_x;
        if (selector == 1) return input->mouse_y;
        return static_cast<std::uint16_t>(input->mouse_button);
    }

    static void native_mouse_set_position(
        void*, std::uint16_t, std::uint16_t) {
        /* The Win32 host applies the requested X recenter after the 36 Hz tick. */
    }

    void play_effect(unsigned effect) {
        if (effect >= SR_SOUND_EFFECT_COUNT) return;
        sr_sound_effect_set_tick(&sound_state, tick_count);
        pending_effect = effect;
        SrSoundEffectHooks hooks{};
        sr_play_sound_effect(
            &sound_state, effect, &speaker_effects, &samples, &hooks);
    }

    void require_file_load(bool condition, const std::string& name) {
        if (!condition) throw std::runtime_error("Could not decode original " + name);
    }

    void load_all() {
        auto exe = read_file(find_original_executable(root));
        auto roads_bytes = read_embedded_file(EmbeddedCampaign::SkyRoads, "roads.lzs");
        auto trek_bytes = read_embedded_file(EmbeddedCampaign::SkyRoads, "trekdat.lzs");
        auto car_bytes = read_embedded_file(EmbeddedCampaign::SkyRoads, "cars.lzs");
        auto dash_bytes = read_embedded_file(EmbeddedCampaign::SkyRoads, "dashbrd.lzs");
        auto music_bytes = read_embedded_file(EmbeddedCampaign::SkyRoads, "muzax.lzs");
        auto sfx_bytes = read_embedded_file(EmbeddedCampaign::SkyRoads, "sfx.snd");

        require_file_load(
            sr_load_road_archive(roads_bytes.data(), roads_bytes.size(), &roads) ==
                SR_ROAD_ARCHIVE_OK,
            "roads.lzs");
        require_file_load(
            sr_load_trek_archive(trek_bytes.data(), trek_bytes.size(), &trek) ==
                SR_TREK_ARCHIVE_OK,
            "trekdat.lzs");
        require_file_load(
            sr_load_car_sprites(car_bytes.data(), car_bytes.size(), &cars) != 0,
            "cars.lzs");
        require_file_load(
            sr_load_renderer_tables_from_exe(
                exe.data(), exe.size(), &renderer_tables) != 0,
            "renderer tables from skyroads.exe");
        require_file_load(
            sr_load_embedded_hud_from_exe(exe.data(), exe.size(), &hud) != 0,
            "HUD tables from skyroads.exe");
        require_file_load(
            sr_load_embedded_sound_effects_from_exe(
                exe.data(), exe.size(), &speaker_effects) != 0,
            "speaker effects from skyroads.exe");
        require_file_load(
            sr_load_sample_effect_archive(sfx_bytes.data(), sfx_bytes.size(), &samples) != 0,
            "sfx.snd");
        require_file_load(
            sr_load_music_archive(music_bytes.data(), music_bytes.size(), &music) ==
                SR_MUSIC_ARCHIVE_OK,
            "muzax.lzs");
        require_file_load(
            sr_load_vga_graphics_archive(
                dash_bytes.data(), dash_bytes.size(), SR_DASHBOARD_PALETTE_BASE,
                &dashboard) == SR_GRAPHICS_ARCHIVE_OK,
            "dashbrd.lzs");

        for (std::size_t index = 0; index < worlds.size(); ++index) {
            auto bytes = read_embedded_file(
                EmbeddedCampaign::SkyRoads,
                "world" + std::to_string(index) + ".lzs");
            require_file_load(
                sr_load_vga_graphics_archive(
                    bytes.data(), bytes.size(), SR_WORLD_PALETTE_BASE,
                    &worlds[index]) == SR_GRAPHICS_ARCHIVE_OK,
                "world" + std::to_string(index) + ".lzs");
        }

        load_xmas_levels();

        custom_level_directory = root / "custom_levels";
        original_level_directory = custom_level_directory / "ORIGINAL LEVELS";
        ensure_demo_custom_levels(custom_level_directory);
        ensure_original_custom_levels();
        custom_levels = load_custom_level_catalog(custom_level_directory);

        load_display("oxy_disp.dat", 10, oxygen_display);
        load_display("ful_disp.dat", 10, fuel_display);
        load_display("speed.dat", 34, speed_display);
        load_main_menu();
        load_settings_menu();
        load_level_menu();
        load_help_pages();
        load_intro_assets();
        demo_record = read_embedded_file(EmbeddedCampaign::SkyRoads, "demo.rec");
    }

    void ensure_original_custom_levels() {
        std::error_code filesystem_error;
        std::filesystem::create_directories(
            original_level_directory, filesystem_error);
        if (filesystem_error) return;

        const auto level_count = std::min<std::size_t>(
            kOriginalLevelCount,
            roads.road_count > 0u ? roads.road_count - 1u : 0u);
        for (std::size_t level_index = 0u;
             level_index < level_count; ++level_index) {
            const auto world = level_index / 3u + 1u;
            const auto road_number = level_index % 3u + 1u;
            const auto name = std::to_string(world) + "-" +
                std::to_string(road_number);
            const auto path = original_level_directory / (name + ".srlevel");

            CustomLevel installed;
            filesystem_error.clear();
            if (std::filesystem::exists(path, filesystem_error) &&
                load_custom_level(path, installed) && installed.name == name) {
                continue;
            }

            const auto& source = roads.roads[level_index + 1u];
            CustomLevel level;
            level.path = path;
            level.name = name;
            level.theme = static_cast<std::uint16_t>(level_index / 3u);
            level.gravity = source.gravity;
            level.fuel = source.fuel;
            level.oxygen = source.oxygen;
            level.cells.assign(
                source.cells,
                source.cells + source.row_count * kCustomRoadColumns);
            (void)save_custom_level(level);
        }
    }

    void load_xmas_levels() {
        if (!embedded_xmas_data_available()) return;
        auto roads_bytes = read_embedded_file(
            EmbeddedCampaign::SkyRoadsXmas, "roads.lzs");
        require_file_load(
            sr_load_road_archive(
                roads_bytes.data(), roads_bytes.size(), &xmas_roads) ==
                SR_ROAD_ARCHIVE_OK &&
                xmas_roads.road_count == kXmasLevelCount + 1u,
            "SkyRoads Xmas roads.lzs");
        for (std::size_t index = 0; index < xmas_worlds.size(); ++index) {
            auto bytes = read_embedded_file(
                EmbeddedCampaign::SkyRoadsXmas,
                "world" + std::to_string(index) + ".lzs");
            require_file_load(
                sr_load_vga_graphics_archive(
                    bytes.data(), bytes.size(), SR_WORLD_PALETTE_BASE,
                    &xmas_worlds[index]) == SR_GRAPHICS_ARCHIVE_OK,
                "SkyRoads Xmas world" + std::to_string(index) + ".lzs");
        }
        xmas_available = true;
    }

    void load_display(const std::string& name, std::size_t count, SrDisplayTable& table) {
        const auto bytes = read_embedded_file(EmbeddedCampaign::SkyRoads, name);
        require_file_load(
            sr_load_display_table(bytes.data(), bytes.size(), count, &table) != 0,
            name);
    }

    void load_main_menu() {
        auto main_bytes = read_embedded_file(
            EmbeddedCampaign::SkyRoads, "mainmenu.lzs");
        require_file_load(
            sr_load_vga_graphics_archive(
                main_bytes.data(), main_bytes.size(), 0xbe, &main_assets.items) ==
                SR_GRAPHICS_ARCHIVE_OK && main_assets.items.picture_count == 3,
            "mainmenu.lzs");
        const auto& first_item = main_assets.items.pictures[0];
        require_file_load(
            main_assets.items.pictures[1].pixel_count == first_item.pixel_count &&
                main_assets.items.pictures[2].pixel_count == first_item.pixel_count,
            "main-menu item geometry");
        main_assets.neutral_items.resize(first_item.pixel_count);
        for (std::size_t pixel = 0; pixel < first_item.pixel_count; ++pixel) {
            const auto first = main_assets.items.pictures[0].pixels[pixel];
            const auto second = main_assets.items.pictures[1].pixels[pixel];
            const auto third = main_assets.items.pictures[2].pixels[pixel];
            main_assets.neutral_items[pixel] = first == second || first == third
                ? first : second == third ? second : 0u;
        }

        AssetStream intro(read_embedded_file(
            EmbeddedCampaign::SkyRoads, "intro.lzs"));
        intro.color_map(main_assets.palette, 0x00);
        auto background = intro.picture(0x00);
        require_file_load(
            sr_copy_picture_vga(main_assets.background.data(), &background.value) != 0,
            "main-menu background");
        intro.color_map(main_assets.palette, 0x32);
        intro.color_map(main_assets.palette, 0x32);
        auto logo = intro.picture(0x32);
        draw_picture(main_assets.background, logo.value);
        /* The third INTRO.LZS picture is the already-highlighted Start menu.
           It belongs to the intro's final frame, not the persistent menu
           backdrop.  Each MAINMENU.LZS state supplies its own complete label
           body and active halo below. */
        copy_palette(main_assets.palette, 0xbe,
            main_assets.items.palette, main_assets.items.palette_count);
    }

    void load_settings_menu() {
        AssetStream stream(read_embedded_file(
            EmbeddedCampaign::SkyRoads, "setmenu.lzs"));
        stream.color_map(settings_assets.palette, 0xc8);
        auto background = stream.picture(0xc8);
        require_file_load(
            sr_copy_picture_vga(settings_assets.background.data(), &background.value) != 0,
            "settings background");
        stream.color_map(settings_assets.palette, 0xfa);
        settings_assets.pictures.reserve(10);
        for (unsigned index = 0; index < 10; ++index) {
            settings_assets.pictures.push_back(stream.picture(0xfa));
        }
    }

    void load_level_menu() {
        AssetStream stream(read_embedded_file(
            EmbeddedCampaign::SkyRoads, "gomenu.lzs"));
        const auto background_palette = stream.color_map_section(0x00);
        apply_palette_section(level_assets.palette, background_palette);
        level_assets.base_color_count = background_palette.count();
        auto background = stream.picture(0x00);
        require_file_load(
            sr_copy_picture_vga(level_assets.background.data(), &background.value) != 0,
            "level-menu background");
        apply_palette_section(
            level_assets.palette, stream.color_map_section(0xf0));
        level_assets.completion_marker = stream.picture(0xf0);
        level_assets.expanded_palette = level_assets.palette;
        if (xmas_available) load_xmas_level_menu_art();
    }

    static std::uint32_t palette_distance(
        const std::uint8_t* first,
        const std::uint8_t* second) {
        std::uint32_t result = 0;
        for (unsigned component = 0; component < 3u; ++component) {
            const int difference = static_cast<int>(first[component]) - second[component];
            result += static_cast<std::uint32_t>(difference * difference);
        }
        return result;
    }

    void load_xmas_level_menu_art() {
        AssetStream stream(read_embedded_file(
            EmbeddedCampaign::SkyRoadsXmas, "gomenu.lzs"));
        const auto xmas_palette = stream.color_map_section(0x00);
        auto background = stream.picture(0x00);
        require_file_load(
            sr_copy_picture_vga(
                level_assets.xmas_background.data(), &background.value) != 0,
            "SkyRoads Xmas level-menu artwork");

        std::array<std::uint32_t, 256> frequency{};
        for (unsigned world = 0; world < 10u; ++world) {
            const unsigned source_x = world < 5u ? 8u : 168u;
            const unsigned source_y = 11u + (world % 5u) * 39u;
            for (unsigned y = 0; y < 28u; ++y) {
                for (unsigned x = 0; x < 52u; ++x) {
                    ++frequency[level_assets.xmas_background[
                        (source_y + y) * kScreenWidth + source_x + x]];
                }
            }
        }

        unsigned combined_count = level_assets.base_color_count;
        while (combined_count < 256u) {
            unsigned best_color = xmas_palette.count();
            std::uint64_t best_score = 0;
            for (unsigned color = 0; color < xmas_palette.count(); ++color) {
                if (frequency[color] == 0) continue;
                const auto* candidate = xmas_palette.colors.data() + color * 3u;
                std::uint32_t closest = std::numeric_limits<std::uint32_t>::max();
                for (unsigned existing = 0; existing < combined_count; ++existing) {
                    closest = std::min(closest, palette_distance(
                        candidate,
                        level_assets.expanded_palette.data() + existing * 3u));
                }
                const auto score = static_cast<std::uint64_t>(frequency[color]) * closest;
                if (score > best_score) {
                    best_score = score;
                    best_color = color;
                }
            }
            if (best_color >= xmas_palette.count() || best_score == 0) break;
            std::memcpy(
                level_assets.expanded_palette.data() + combined_count * 3u,
                xmas_palette.colors.data() + best_color * 3u, 3u);
            ++combined_count;
        }

        std::array<std::uint8_t, 256> remap{};
        for (unsigned color = 0; color < xmas_palette.count(); ++color) {
            const auto* candidate = xmas_palette.colors.data() + color * 3u;
            std::uint32_t closest = std::numeric_limits<std::uint32_t>::max();
            unsigned closest_index = 0;
            for (unsigned existing = 0; existing < combined_count; ++existing) {
                const auto distance = palette_distance(
                    candidate,
                    level_assets.expanded_palette.data() + existing * 3u);
                if (distance < closest) {
                    closest = distance;
                    closest_index = existing;
                }
            }
            remap[color] = static_cast<std::uint8_t>(closest_index);
        }
        for (auto& pixel : level_assets.xmas_background) {
            if (pixel >= xmas_palette.count()) {
                throw std::runtime_error(
                    "SkyRoads Xmas menu pixel exceeds its original palette");
            }
            pixel = remap[pixel];
        }
    }

    void load_help_pages() {
        AssetStream stream(read_embedded_file(
            EmbeddedCampaign::SkyRoads, "helpmenu.lzs"));
        for (auto& page : help_pages) {
            stream.color_map(page.palette, 0xc8);
            auto picture = stream.picture(0xc8);
            require_file_load(
                sr_copy_picture_vga(page.framebuffer.data(), &picture.value) != 0,
                "help page");
        }
    }

    void load_intro_assets() {
        AssetStream stream(read_embedded_file(
            EmbeddedCampaign::SkyRoads, "intro.lzs"));
        intro_assets.background_palette = stream.color_map_section(0x00);
        auto background = stream.picture(0x00);
        require_file_load(
            sr_copy_picture_vga(intro_assets.background.data(), &background.value) != 0,
            "intro background");
        intro_assets.logo_palette_from = stream.color_map_section(0x32);
        intro_assets.logo_palette_to = stream.color_map_section(0x32);
        intro_assets.logo = stream.picture(0x32);
        for (std::size_t index = 0; index < intro_assets.cards.size(); ++index) {
            intro_assets.card_palette_from[index] = stream.color_map_section(0xa0);
            intro_assets.card_palette_to[index] = stream.color_map_section(0xa0);
            intro_assets.cards[index] = stream.picture(0xa0);
        }

        const auto animation_bytes = read_embedded_file(
            EmbeddedCampaign::SkyRoads, "anim.lzs");
        require_file_load(
            sr_load_animation_archive(
                animation_bytes.data(), animation_bytes.size(),
                &intro_assets.animation) == SR_ANIMATION_ARCHIVE_OK,
            "anim.lzs");
        intro_assets.sample = read_embedded_file(
            EmbeddedCampaign::SkyRoads, "intro.snd");

        copy_palette(
            intro_animation_palette, 0, intro_assets.animation.palette,
            intro_assets.animation.palette_count);
        /* 1000:4739 first copies the ANIM CMAP into the full 256-color
         * descriptor at 5182.  The final intro fade later overlays only the
         * background and logo ranges at 1000:4A7B/4A88. */
        intro_final_palette = intro_animation_palette;
        apply_palette_section(intro_final_palette, intro_assets.background_palette);
        apply_palette_section(intro_final_palette, intro_assets.logo_palette_to);
    }

    void load_config_file() {
        bool valid = false;
        try {
            const auto bytes = read_file(find_file(root, "SKYROADS.CFG"));
            if (bytes.size() == sizeof(config_file)) {
                std::memcpy(&config_file, bytes.data(), sizeof(config_file));
                auto checked = config_file;
                valid = sr_update_config_checksum(&checked) == 0;
            }
        }
        catch (const std::exception&) {
            valid = false;
        }
        if (!valid) std::memset(&config_file, 0, sizeof(config_file));
        selected_input_mode = config_file.values[0];
        sound_disabled = config_file.values[1];
        for (std::size_t index = 0; index < kOriginalLevelCount; ++index) {
            completion_count[index] = config_file.values[index + 2u];
        }
        if (!xmas_available) return;

        valid = false;
        try {
            const auto bytes = read_file(find_file(root, "SKYXMAS.CFG"));
            if (bytes.size() == sizeof(xmas_config_file)) {
                std::memcpy(
                    &xmas_config_file, bytes.data(), sizeof(xmas_config_file));
                auto checked = xmas_config_file;
                valid = sr_update_config_checksum(&checked) == 0;
            }
        }
        catch (const std::exception&) {
            valid = false;
        }
        if (!valid) std::memset(&xmas_config_file, 0, sizeof(xmas_config_file));
        for (std::size_t index = 0; index < kXmasLevelCount; ++index) {
            completion_count[kOriginalLevelCount + index] =
                xmas_config_file.values[index + 2u];
        }
    }

    void load_native_config_file() {
        std::ifstream stream(root / "SKYROADS.NATIVE.CFG");
        std::string line;
        if (!stream || !std::getline(stream, line) ||
            line != "SKYROADS NATIVE 1") return;
        while (std::getline(stream, line)) {
            if (line == "high_definition=1") high_definition = true;
            else if (line == "high_definition=0") high_definition = false;
        }
    }

    void save_native_config_file() const {
        std::ofstream stream(root / "SKYROADS.NATIVE.CFG", std::ios::trunc);
        if (!stream) return;
        stream << "SKYROADS NATIVE 1\n"
               << "high_definition=" << (high_definition ? 1 : 0) << '\n';
    }

    void save_config_file() {
        config_file.values[0] = selected_input_mode;
        config_file.values[1] = sound_disabled;
        for (std::size_t index = 0; index < kOriginalLevelCount; ++index) {
            config_file.values[index + 2u] = completion_count[index];
        }
        sr_update_config_checksum(&config_file);
        std::ofstream stream(root / "SKYROADS.CFG", std::ios::binary | std::ios::trunc);
        if (stream) {
            stream.write(reinterpret_cast<const char*>(&config_file), sizeof(config_file));
        }
        if (xmas_available) {
            xmas_config_file.values[0] = selected_input_mode;
            xmas_config_file.values[1] = sound_disabled;
            for (std::size_t index = 0; index < kXmasLevelCount; ++index) {
                xmas_config_file.values[index + 2u] =
                    completion_count[kOriginalLevelCount + index];
            }
            sr_update_config_checksum(&xmas_config_file);
            std::ofstream xmas_stream(
                root / "SKYXMAS.CFG", std::ios::binary | std::ios::trunc);
            if (xmas_stream) {
                xmas_stream.write(
                    reinterpret_cast<const char*>(&xmas_config_file),
                    sizeof(xmas_config_file));
            }
        }
        save_native_config_file();
    }

    void present(const std::array<std::uint8_t, kPaletteSize>& palette) {
        active_palette = palette;
        if (palette_trace_enabled && active_palette != palette_trace_previous) {
            palette_trace.insert(
                palette_trace.end(), active_palette.begin(), active_palette.end());
            palette_trace_previous = active_palette;
        }
        for (std::size_t pixel = 0; pixel < indexed.size(); ++pixel) {
            const std::size_t color = indexed[pixel] * 3u;
            const auto expand = [](std::uint8_t value) -> std::uint32_t {
                return static_cast<std::uint32_t>((value << 2u) | (value >> 4u));
            };
            rgba[pixel] = (expand(palette[color]) << 16u) |
                (expand(palette[color + 1u]) << 8u) |
                expand(palette[color + 2u]);
        }
        ++presentation_revision;
    }

    static bool any_intro_key(const NativeInput& input) {
        return input.left || input.right || input.up || input.down || input.jump ||
            input.enter_pressed || input.escape_pressed;
    }

    PaletteSection white_like(const PaletteSection& section) const {
        PaletteSection white;
        white.base = section.base;
        white.colors.assign(section.colors.size(), 0x3f);
        return white;
    }

    void start_intro() {
        active_screen = NativeScreen::Intro;
        sr_intro_sequence_init(&intro_sequence);
        indexed = intro_assets.background;
        pending_intro_sample = false;
        sr_opl_load_track(&opl, &music, 0, sound_disabled != 0);
        present(black_palette);
    }

    void enter_main_menu(bool fade_in = false) {
        active_screen = NativeScreen::MainMenu;
        sr_main_menu_init(&main_state);
        main_selection = 0;
        if (fade_in) sr_menu_flow_enter_main(&menu_flow);
        else sr_menu_flow_after_intro(&menu_flow);
        sr_opl_load_track(&opl, &music, 1, sound_disabled != 0);
        render_main_menu();
        if (fade_in) present(black_palette);
    }

    void draw_intro_animation_group(unsigned group_index) {
        if (group_index >= intro_assets.animation.group_count) {
            throw std::runtime_error("Original intro animation group is out of range");
        }
        const auto& group = intro_assets.animation.groups[group_index];
        for (std::size_t part = 0; part < group.frame_count; ++part) {
            const auto& frame = intro_assets.animation.frames[group.first_frame + part];
            require_file_load(
                sr_copy_picture_vga(indexed.data(), &frame.picture) != 0,
                "intro animation frame");
        }
        present(active_palette);
    }

    void draw_intro_logo(unsigned offset) {
        const auto& logo = intro_assets.logo.value;
        if (logo.width != kScreenWidth || (logo.height & 1u) != 0 || offset >= logo.width) {
            throw std::runtime_error("Original intro logo has unexpected scanline geometry");
        }
        SrPictureBlitter blitter{};
        blitter.reference = intro_assets.background.data();
        blitter.destination = indexed.data();
        blitter.destination_size = indexed.size();
        blitter.restore_below = 1;
        blitter.transparent_below = 0;
        for (std::size_t row = 0; row < logo.height; row += 2u) {
            const std::size_t destination = logo.screen_offset + row * kScreenWidth;
            const std::size_t source = row * logo.width;
            require_file_load(
                sr_compose_picture_row(
                    &blitter, destination, logo.pixels + source, logo.width,
                    offset, 0) != 0,
                "intro logo even scanline");
            require_file_load(
                sr_compose_picture_row(
                    &blitter, destination + kScreenWidth,
                    logo.pixels + source + logo.width + offset, logo.width,
                    0, offset) != 0,
                "intro logo odd scanline");
        }
        present(active_palette);
    }

    void draw_intro_card(unsigned card_index, bool clear) {
        if (card_index >= intro_assets.cards.size()) {
            throw std::runtime_error("Original intro credit card is out of range");
        }
        SrPictureBlitter blitter{};
        blitter.reference = intro_assets.background.data();
        blitter.destination = indexed.data();
        blitter.destination_size = indexed.size();
        blitter.restore_below = static_cast<std::uint8_t>(clear ? 0xff : 1);
        blitter.transparent_below = 0;
        require_file_load(
            sr_draw_picture_composited(
                &blitter, &intro_assets.cards[card_index].value) != 0,
            clear ? "intro credit clear" : "intro credit draw");
        present(active_palette);
    }

    void intro_tick(const NativeInput& input) {
        const auto prior_phase = intro_sequence.phase;
        const auto prior_card = intro_sequence.card_index;
        const auto step = sr_intro_sequence_tick(
            &intro_sequence, any_intro_key(input));
        if ((step.events & SR_INTRO_EVENT_ABORTED) != 0) {
            enter_main_menu(false);
            return;
        }

        std::array<std::uint8_t, kPaletteSize> palette{};
        if (prior_phase == SR_INTRO_FADE_IN) {
            blend_full_palette(
                palette, black_palette, intro_animation_palette,
                step.palette_percent);
            present(palette);
        }
        else if (prior_phase == SR_INTRO_LOGO_TO_WHITE) {
            palette = active_palette;
            const auto white = white_like(intro_assets.logo_palette_from);
            blend_palette_section(
                palette, intro_assets.logo_palette_from, white,
                step.palette_percent);
            present(palette);
        }
        else if (prior_phase == SR_INTRO_LOGO_FROM_WHITE) {
            palette = active_palette;
            const auto white = white_like(intro_assets.logo_palette_to);
            blend_palette_section(
                palette, white, intro_assets.logo_palette_to,
                step.palette_percent);
            present(palette);
        }
        else if (prior_phase == SR_INTRO_CARD_FADE_IN) {
            palette = active_palette;
            blend_palette_section(
                palette,
                intro_assets.card_palette_from[prior_card],
                intro_assets.card_palette_to[prior_card],
                step.palette_percent);
            present(palette);
        }
        else if (prior_phase == SR_INTRO_CARD_FADE_OUT) {
            palette = active_palette;
            blend_palette_section(
                palette,
                intro_assets.card_palette_to[prior_card],
                intro_assets.card_palette_from[prior_card],
                step.palette_percent);
            present(palette);
        }
        else if (prior_phase == SR_INTRO_FINAL_FADE_OUT) {
            blend_full_palette(
                palette, intro_final_palette, black_palette,
                step.palette_percent);
            present(palette);
        }

        if ((step.events & SR_INTRO_EVENT_START_SAMPLE) != 0) {
            pending_intro_sample = true;
        }
        if ((step.events & SR_INTRO_EVENT_DRAW_ANIMATION_GROUP) != 0) {
            draw_intro_animation_group(step.animation_group);
        }
        if ((step.events & SR_INTRO_EVENT_DRAW_LOGO) != 0) {
            if (prior_phase == SR_INTRO_LOGO_WAIT) {
                apply_palette_section(active_palette, intro_assets.logo_palette_from);
            }
            draw_intro_logo(step.logo_offset);
        }
        if ((step.events & SR_INTRO_EVENT_CLEAR_CARD) != 0) {
            draw_intro_card(prior_card, true);
        }
        if ((step.events & SR_INTRO_EVENT_BEGIN_CARD) != 0) {
            apply_palette_section(
                active_palette, intro_assets.card_palette_from[step.card_index]);
            draw_intro_card(step.card_index, false);
        }
        if ((step.events & SR_INTRO_EVENT_BEGIN_FINAL_FADE) != 0) {
            present(intro_final_palette);
        }
        if ((step.events & SR_INTRO_EVENT_FINISHED) != 0) {
            start_demo();
        }
    }

    void render_main_menu() {
        indexed = main_assets.background;
        if (main_selection < SR_MAIN_MENU_ITEM_COUNT) {
            main_state.selection = static_cast<std::uint16_t>(main_selection);
            draw_picture(indexed, main_assets.items.pictures[main_selection]);
        }
        else {
            const auto& picture = main_assets.items.pictures[0];
            for (std::size_t row = 0; row < picture.height; ++row) {
                for (std::size_t column = 0; column < picture.width; ++column) {
                    const auto pixel = main_assets.neutral_items[
                        row * picture.width + column];
                    if (pixel != 0u) {
                        indexed[picture.screen_offset + row * kScreenWidth + column] =
                            pixel;
                    }
                }
            }
        }
        draw_main_menu_editor_label(indexed, main_selection == 3u);
        present(main_assets.palette);
    }

    const std::array<std::uint8_t, kPaletteSize>& custom_menu_palette() const {
        return xmas_available ? level_assets.expanded_palette : level_assets.palette;
    }

    std::size_t custom_browser_level_offset() const {
        return custom_browser_original_levels ? 1u : 2u;
    }

    std::size_t custom_browser_total_items() const {
        return custom_levels.size() + custom_browser_level_offset();
    }

    bool custom_browser_selection_is_level() const {
        const auto offset = custom_browser_level_offset();
        return custom_browser_selection >= offset &&
            custom_browser_selection - offset < custom_levels.size();
    }

    std::size_t custom_browser_level_index() const {
        return custom_browser_selection - custom_browser_level_offset();
    }

    void render_custom_browser() {
        draw_original_menu_backdrop(indexed, level_assets.background.data());
        draw_native_text(indexed, 8u, 7u,
            custom_browser_original_levels ? "ORIGINAL LEVELS" : "CUSTOM ROADS", 1u);
        draw_native_text(indexed, 211u, 7u,
            fixed_number(custom_levels.size(), 3u) +
                (custom_browser_original_levels ? " ROADS" : " CREATIONS"), 3u);

        constexpr std::size_t visible_items = 13u;
        const auto total_items = custom_browser_total_items();
        const auto first_item = (custom_browser_selection / visible_items) * visible_items;
        for (std::size_t slot = 0; slot < visible_items; ++slot) {
            const auto item = first_item + slot;
            if (item >= total_items) break;
            const auto y = static_cast<unsigned>(24u + slot * 11u);
            const bool selected = item == custom_browser_selection;
            if (selected) {
                draw_native_rectangle(indexed, 8u, y - 1u, 190u, 9u, 1u);
            }
            std::string label;
            if (custom_browser_original_levels) {
                label = item == 0u ? "BACK TO CREATIONS" :
                    custom_levels[item - 1u].name;
            }
            else if (item == 0u) label = "CREATE NEW";
            else if (item == 1u) label = "ORIGINAL LEVELS";
            else label = custom_levels[item - 2u].name;
            draw_native_text(indexed, 12u, y, label, selected ? 1u : 2u);
        }

        if (custom_browser_selection_is_level()) {
            const auto& level = custom_levels[custom_browser_level_index()];
            draw_native_text(indexed, 211u, 31u,
                "ROWS " + fixed_number(level.row_count(), 3u), 3u);
            draw_native_text(indexed, 211u, 42u,
                "THEME " + fixed_number(level.theme + 1u, 2u), 3u);
            draw_native_text(indexed, 211u, 53u,
                "GRAVITY " + fixed_number(level.gravity, 2u), 3u);
            draw_native_text(indexed, 211u, 75u, "ENTER EDIT", 2u);
            draw_native_text(indexed, 211u, 86u, "P PLAY TEST", 2u);
        }
        else if (custom_browser_original_levels) {
            draw_native_text(indexed, 211u, 31u, "CUSTOM ROADS", 3u);
            draw_native_text(indexed, 211u, 53u, "ENTER BACK", 2u);
        }
        else if (custom_browser_selection == 1u) {
            draw_native_text(indexed, 211u, 31u, "30 ORIGINAL ROADS", 3u);
            draw_native_text(indexed, 211u, 42u, "1-1 TO 10-3", 3u);
            draw_native_text(indexed, 211u, 64u, "ENTER OPEN", 2u);
        }
        else {
            draw_native_text(indexed, 211u, 31u, "NEW ROAD", 3u);
            draw_native_text(indexed, 211u, 42u, "96 ROWS", 3u);
            draw_native_text(indexed, 211u, 64u, "ENTER CREATE", 2u);
        }
        draw_native_text(indexed, 8u, 184u,
            "UP DOWN SELECT  PGUP PGDN PAGE  ESC BACK", 3u);
        present(custom_menu_palette());
    }

    void draw_editor_tile(
        unsigned x,
        unsigned y,
        unsigned width,
        unsigned height,
        unsigned material,
        unsigned shape,
        bool selected) {
        const auto color = editor_material_color(material);
        fill_native_rectangle(indexed, x, y, width, height, 0u);
        if (material != 0u && width > 2u && height > 2u) {
            fill_native_rectangle(indexed, x + 1u, y + 1u,
                width - 2u, height - 2u, color);
        }

        if (material == 2u) {
            for (unsigned stripe = 3u; stripe + 1u < width; stripe += 4u) {
                fill_native_rectangle(indexed, x + stripe, y + 2u, 1u,
                    height > 4u ? height - 4u : 1u, 0u);
            }
        }
        else if (material == 8u) {
            for (unsigned stripe = 2u; stripe + 1u < width; stripe += 5u) {
                fill_native_rectangle(indexed, x + stripe, y + 2u, 1u, 1u, 1u);
                if (height > 5u && stripe + 1u < width) {
                    fill_native_rectangle(indexed, x + stripe + 1u, y + 4u, 1u, 1u, 1u);
                }
            }
        }
        else if (material == 9u) {
            fill_native_rectangle(indexed, x + width / 2u, y + 2u, 1u,
                height > 4u ? height - 4u : 1u, 1u);
            fill_native_rectangle(indexed, x + width / 2u - 2u,
                y + height / 2u, 5u, 1u, 1u);
        }
        else if (material == 10u) {
            fill_native_rectangle(indexed, x + width / 2u - 3u,
                y + height / 2u, 7u, 1u, 1u);
            fill_native_rectangle(indexed, x + width / 2u + 2u,
                y + height / 2u - 1u, 1u, 3u, 1u);
            fill_native_rectangle(indexed, x + width / 2u + 3u,
                y + height / 2u, 1u, 1u, 1u);
        }
        else if (material == 12u) {
            const auto limit = std::min(width, height);
            for (unsigned diagonal = 2u; diagonal + 2u < limit; ++diagonal) {
                fill_native_rectangle(indexed, x + diagonal, y + diagonal, 1u, 1u, 1u);
                fill_native_rectangle(indexed, x + width - 1u - diagonal,
                    y + diagonal, 1u, 1u, 1u);
            }
        }

        if (shape == 1u && width > 12u && height > 4u) {
            fill_native_rectangle(indexed, x + 6u, y + 3u,
                width - 12u, height - 3u, 0u);
        }
        else if (shape == 2u && height > 4u) {
            fill_native_rectangle(indexed, x + 2u, y + height / 2u,
                width > 4u ? width - 4u : 1u, 2u, 1u);
        }
        else if (shape == 3u && height > 5u) {
            fill_native_rectangle(indexed, x + 3u, y + 2u,
                width > 6u ? width - 6u : 1u, 1u, 1u);
            fill_native_rectangle(indexed, x + 6u, y + 4u,
                width > 12u ? width - 12u : 1u, 1u, 1u);
        }
        else if (shape == 4u && width > 8u && height > 5u) {
            draw_native_rectangle(indexed, x + 4u, y + 2u,
                width - 8u, height - 4u, 1u);
        }
        else if (shape == 5u && width > 10u && height > 5u) {
            fill_native_rectangle(indexed, x + 3u, y + 2u,
                width - 6u, 1u, 1u);
            fill_native_rectangle(indexed, x + 6u, y + 4u,
                width - 12u, 1u, 1u);
            fill_native_rectangle(indexed, x + width / 2u, y + 2u,
                1u, height - 3u, 1u);
        }

        draw_native_rectangle(indexed, x, y, width, height,
            selected ? 1u : material == 0u ? 200u : color);
    }

    void render_custom_editor() {
        if (custom_editor_index >= custom_levels.size()) return;
        const auto& level = custom_levels[custom_editor_index];
        draw_original_menu_backdrop(indexed, level_assets.background.data());
        draw_native_text(indexed, 4u, 3u, "EDIT " + level.name, 1u);
        draw_native_text(indexed, 213u, 3u,
            custom_editor_dirty ? "UNSAVED" : custom_editor_status, 3u);
        draw_native_text(indexed, 4u, 13u,
            custom_editor_view == EditorViewMode::Top
                ? "ARROWS MOVE CLICK SPACE PAINT V"
                : "ARROWS MOVE SPACE PAINT  V VIEW",
            3u);

        constexpr std::size_t visible_rows = 15u;
        const auto first_row = (custom_editor_row / visible_rows) * visible_rows;
        if (custom_editor_view == EditorViewMode::Top) {
            for (std::size_t column = 0; column < kCustomRoadColumns; ++column) {
                draw_native_text(indexed,
                    static_cast<unsigned>(32u + column * 25u), 22u,
                    fixed_number(column + 1u, 1u), 3u);
            }
            for (std::size_t visible = 0; visible < visible_rows; ++visible) {
                const auto row = first_row + visible;
                if (row >= level.row_count()) break;
                const auto y = static_cast<unsigned>(31u + visible * 10u);
                draw_native_text(indexed, 0u, y + 1u,
                    fixed_number(row + 1u, 3u), row == custom_editor_row ? 1u : 3u);
                for (std::size_t column = 0; column < kCustomRoadColumns; ++column) {
                    const auto x = static_cast<unsigned>(24u + column * 25u);
                    const auto cell = level.cells[row * kCustomRoadColumns + column];
                    const auto material = static_cast<unsigned>(cell & 0x0fu);
                    const auto shape = static_cast<unsigned>((cell >> 8u) & 0x0fu);
                    draw_editor_tile(x, y, 23u, 9u, material, shape,
                        row == custom_editor_row && column == custom_editor_column);
                }
            }
        }
        else {
            require_file_load(
                render_editor_spatial_view(
                    indexed, level, first_row, custom_editor_row,
                    custom_editor_column, custom_editor_view),
                "native 3D editor view");
            draw_native_text(indexed, 4u, 23u,
                "V " + std::string(editor_view_name(custom_editor_view)) +
                "  ROW " + fixed_number(custom_editor_row + 1u, 3u), 1u);
        }

        draw_native_text(indexed, 207u, 22u, "MATERIAL TOOLS", 1u);
        for (std::size_t slot = 0; slot < kEditorMaterials.size(); ++slot) {
            const auto y = static_cast<unsigned>(31u + slot * 11u);
            const bool selected =
                custom_editor_brush_material == kEditorMaterials[slot];
            draw_editor_tile(
                207u, y, 13u, 9u, kEditorMaterials[slot], 0u, selected);
            draw_native_text(indexed, 224u, y + 1u,
                fixed_number(slot, 1u) + " " + material_tool_name(slot),
                selected ? 1u : 3u);
        }
        draw_editor_tile(207u, 144u, 20u, 9u,
            custom_editor_brush_material, custom_editor_brush_shape, true);
        draw_native_text(indexed, 232u, 145u,
            "T " + shape_name(custom_editor_brush_shape), 2u);
        draw_native_text(indexed, 207u, 156u,
            "W THEME " + fixed_number(level.theme + 1u, 2u), 3u);
        draw_native_text(indexed, 207u, 167u,
            "G " + fixed_number(level.gravity, 2u) +
            " F " + fixed_number(level.fuel, 3u) +
            " O " + fixed_number(level.oxygen, 3u), 3u);
        draw_native_text(indexed, 2u, 188u,
            "V " + std::string(editor_view_name(custom_editor_view)) +
            " PGUP PGDN ESC", 3u);
        draw_native_text(indexed, 207u, 178u, "INS DEL ROW", 3u);
        draw_native_text(indexed, 207u, 189u, "S SAVE P PLAY", 2u);
        present(custom_menu_palette());
    }

    void reload_custom_levels() {
        custom_levels = load_custom_level_catalog(
            custom_browser_original_levels
                ? original_level_directory : custom_level_directory);
        if (custom_browser_original_levels) {
            std::sort(custom_levels.begin(), custom_levels.end(),
                [](const auto& left, const auto& right) {
                    if (left.theme != right.theme) return left.theme < right.theme;
                    return left.name < right.name;
                });
        }
        custom_browser_selection = std::min(
            custom_browser_selection, custom_browser_total_items() - 1u);
    }

    void enter_custom_browser() {
        reload_custom_levels();
        active_screen = NativeScreen::CustomLevelBrowser;
        sr_opl_load_track(&opl, &music, 1u, sound_disabled != 0);
        render_custom_browser();
    }

    void enter_custom_editor(std::size_t index) {
        if (index >= custom_levels.size()) return;
        custom_editor_index = index;
        custom_editor_row = std::min(
            custom_editor_row, custom_levels[index].row_count() - 1u);
        custom_editor_column = std::min(
            custom_editor_column, kCustomRoadColumns - 1u);
        const auto cell = custom_levels[index].cells[
            custom_editor_row * kCustomRoadColumns + custom_editor_column];
        custom_editor_brush_material = static_cast<std::uint16_t>(cell & 0x0fu);
        custom_editor_brush_shape = static_cast<std::uint16_t>(
            (cell >> 8u) & 0x0fu);
        custom_editor_dirty = false;
        custom_editor_status = "READY";
        active_screen = NativeScreen::CustomLevelEditor;
        sr_opl_load_track(&opl, &music, 1u, sound_disabled != 0);
        render_custom_editor();
    }

    void create_custom_level() {
        std::filesystem::path path;
        std::string name;
        for (unsigned number = 1u; number <= 999u; ++number) {
            name = "CUSTOM " + fixed_number(number, 3u);
            path = custom_level_directory /
                ("custom_" + fixed_number(number, 3u) + ".srlevel");
            std::error_code error;
            if (!std::filesystem::exists(path, error)) break;
        }
        auto level = make_blank_custom_level(path, name);
        if (!save_custom_level(level)) {
            custom_editor_status = "SAVE FAILED";
            render_custom_browser();
            return;
        }
        reload_custom_levels();
        const auto found = std::find_if(
            custom_levels.begin(), custom_levels.end(),
            [&](const auto& candidate) { return candidate.path == path; });
        if (found != custom_levels.end()) {
            custom_browser_selection = static_cast<std::size_t>(
                std::distance(custom_levels.begin(), found)) +
                custom_browser_level_offset();
            custom_editor_row = 0u;
            custom_editor_column = 3u;
            enter_custom_editor(custom_browser_level_index());
        }
    }

    void save_custom_editor() {
        if (custom_editor_index >= custom_levels.size()) return;
        std::string error;
        if (save_custom_level(custom_levels[custom_editor_index], &error)) {
            custom_editor_dirty = false;
            custom_editor_status = "SAVED";
        }
        else {
            custom_editor_status = "SAVE FAILED";
        }
    }

    void render_settings() {
        indexed = settings_assets.background;
        for (unsigned item = 0; item < 5; ++item) {
            const bool enabled = item < 3
                ? selected_input_mode == item
                : sound_disabled + 3u == item;
            if (enabled) draw_picture(indexed, settings_assets.pictures[item + 5u].value);
        }
        if (settings_selection < SR_SETTINGS_MENU_ITEM_COUNT) {
            draw_picture(indexed, settings_assets.pictures[settings_selection].value);
        }
        const auto display_color = static_cast<std::uint8_t>(
            settings_selection == 5u ? 0xfcu : 0xfbu);
        draw_native_rectangle(indexed, 12u, 144u, 76u, 32u, display_color);
        draw_native_text(indexed, 25u, 150u, "HI DEF", display_color);
        draw_native_text(indexed, 31u, 162u,
            high_definition ? "ON" : "OFF",
            high_definition ? 0xfcu : 0xfbu);
        present(settings_assets.palette);
    }

    void render_level_menu() {
        if (xmas_available) {
            require_file_load(
                render_expanded_level_menu(
                    indexed,
                    level_assets.background.data(),
                    level_assets.xmas_background.data(),
                    completion_count.data(), available_level_count(),
                    level_state.current_level),
                "four-column level menu");
            present(level_assets.expanded_palette);
            return;
        }
        indexed = level_assets.background;
        for (unsigned level = 0; level < SR_LEVEL_COUNT; ++level) {
            SrLevelMenuLayout layout{};
            sr_level_menu_layout(level, &layout);
            const auto count = sr_level_completion_marker_count(completion_count[level]);
            for (unsigned marker = 0; marker < count; ++marker) {
                draw_picture(indexed, level_assets.completion_marker.value,
                    static_cast<std::uint16_t>(
                        layout.completion_y * kScreenWidth +
                        layout.completion_x + marker * 7u));
            }
        }
        SrLevelMenuLayout layout{};
        sr_level_menu_layout(level_state.current_level, &layout);
        for (unsigned y = 0; y < 9; ++y) {
            for (unsigned x = 0; x < 48; ++x) {
                if (y == 0 || y == 8 || x == 0 || x == 47) {
                    indexed[(layout.selector_y + y) * kScreenWidth +
                        layout.selector_x + x] = 1;
                }
            }
        }
        present(level_assets.palette);
    }

    void render_help() {
        indexed = help_pages[help_page].framebuffer;
        present(help_pages[help_page].palette);
    }

    void enter_level_selection(bool fade_in = true) {
        active_screen = NativeScreen::LevelSelection;
        sr_level_menu_init(&level_state,
            static_cast<std::uint16_t>(std::min<std::size_t>(
                road_index, available_level_count() - 1u)));
        if (fade_in) sr_menu_flow_enter_levels(&menu_flow);
        sr_opl_load_track(&opl, &music, 1, sound_disabled != 0);
        render_level_menu();
        if (fade_in) present(black_palette);
    }

    const std::array<std::uint8_t, kPaletteSize>& menu_palette(
        SrMenuFlowScreen screen) const {
        if (screen == SR_MENU_FLOW_SETTINGS) return settings_assets.palette;
        if (screen == SR_MENU_FLOW_HELP_ONE) return help_pages[0].palette;
        if (screen == SR_MENU_FLOW_HELP_TWO) return help_pages[1].palette;
        if (screen == SR_MENU_FLOW_LEVELS) {
            return xmas_available
                ? level_assets.expanded_palette : level_assets.palette;
        }
        return main_assets.palette;
    }

    void draw_routed_menu(
        SrMenuFlowScreen screen,
        SrMenuFlowScreen previous) {
        if (screen == SR_MENU_FLOW_MAIN) {
            active_screen = NativeScreen::MainMenu;
            if (previous == SR_MENU_FLOW_LEVELS) {
                sr_main_menu_init(&main_state);
                main_selection = 0;
            }
            sr_opl_load_track(&opl, &music, 1, sound_disabled != 0);
            render_main_menu();
        }
        else if (screen == SR_MENU_FLOW_SETTINGS) {
            active_screen = NativeScreen::Settings;
            sr_settings_menu_init(
                &settings_state, selected_input_mode, sound_disabled);
            settings_selection = settings_state.selection;
            render_settings();
        }
        else if (screen == SR_MENU_FLOW_HELP_ONE ||
            screen == SR_MENU_FLOW_HELP_TWO) {
            active_screen = NativeScreen::Help;
            help_page = screen == SR_MENU_FLOW_HELP_TWO ? 1u : 0u;
            render_help();
        }
        else {
            active_screen = NativeScreen::LevelSelection;
            sr_level_menu_init(&level_state,
                static_cast<std::uint16_t>(std::min<std::size_t>(
                    road_index, available_level_count() - 1u)));
            sr_opl_load_track(&opl, &music, 1, sound_disabled != 0);
            render_level_menu();
        }
    }

    void menu_transition_tick() {
        const auto previous_screen = menu_flow.screen;
        const auto previous_phase = menu_flow.phase;
        const auto step = sr_menu_flow_tick(&menu_flow);
        if (previous_phase == SR_MENU_FLOW_FADE_IN ||
            previous_phase == SR_MENU_FLOW_FADE_OUT) {
            std::array<std::uint8_t, kPaletteSize> palette{};
            blend_full_palette(
                palette, black_palette, menu_palette(previous_screen),
                step.palette_percent);
            present(palette);
        }
        if ((step.events & SR_MENU_FLOW_EVENT_DRAW_SCREEN) != 0) {
            draw_routed_menu(step.screen, previous_screen);
            present(black_palette);
        }
        if ((step.events & SR_MENU_FLOW_EVENT_START_LEVEL) != 0) {
            start_level(level_state.current_level);
        }
    }

    void begin_road(
        const SrRoadData& road,
        std::size_t world_index,
        NativeScreen screen,
        bool final_unfinished_level,
        bool xmas_campaign = false) {
        const auto& campaign_worlds = xmas_campaign ? xmas_worlds : worlds;
        if (world_index >= campaign_worlds.size()) return;
        active_xmas_campaign = xmas_campaign;
        const auto& world = campaign_worlds[world_index];
        if (dashboard.picture_count == 0 || world.picture_count == 0) {
            throw std::runtime_error("Original gameplay background is incomplete");
        }
        require_file_load(
            sr_build_gameplay_palette_vga(
                gameplay_palette.data(), &road, &cars, &dashboard, &world) != 0,
            "gameplay palette");
        require_file_load(
            sr_build_gameplay_background_vga(
                gameplay_background.data(), &world.pictures[0],
                &dashboard.pictures[0]) != 0,
            "gameplay background");
        require_file_load(
            sr_draw_dashboard_gravity_vga(
                gameplay_background.data(), &hud, road.gravity) != 0,
            "dashboard gravity display");

        gameplay_config = {};
        gameplay_config.road_gravity = road.gravity;
        gameplay_config.road_oxygen = road.oxygen;
        gameplay_config.road_fuel = road.fuel;
        gameplay_config.road_length_rows = static_cast<std::uint16_t>(road.row_count);
        gameplay_config.collision_resolution_enabled = 1;
        gameplay_config.forward_speed_limit = overdrive_cheat
            ? kOverdriveForwardSpeedLimit : kOriginalForwardSpeedLimit;
        sr_gameplay_init(&gameplay, &gameplay_config);
        if (no_gravity_cheat) gameplay.gravity_step = 0;
        sr_dashboard_state_init(&dashboard_state);
        sr_vga_renderer_state_init(&renderer_state);
        indexed = gameplay_background;
        run_level_play_screen = screen;
        sr_run_level_init(&run_level_state, final_unfinished_level);
        active_screen = NativeScreen::LevelTransition;
        tick_count = 0;
        finish_coast_active = false;
        previous_jump_control = false;
        live_input = {};
        live_input.mode = static_cast<SrInputMode>(selected_input_mode);
        live_input.joystick_center_x = latest_input.joystick_connected
            ? latest_input.joystick_x : 0x8000u;
        live_input.joystick_center_y = latest_input.joystick_connected
            ? latest_input.joystick_y : 0x8000u;
        active_palette = black_palette;
        render_gameplay();
    }

    void start_road(
        std::size_t road_record,
        std::size_t world_index,
        NativeScreen screen,
        bool final_unfinished_level,
        bool xmas_campaign = false) {
        const auto& campaign_roads = xmas_campaign ? xmas_roads : roads;
        if (road_record >= campaign_roads.road_count) return;
        active_custom_playtest = false;
        active_road_record = road_record;
        begin_road(
            campaign_roads.roads[active_road_record], world_index, screen,
            final_unfinished_level, xmas_campaign);
    }

    const SrRoadData& current_road() const {
        if (active_custom_playtest) return custom_play_road;
        const auto& campaign_roads = active_xmas_campaign ? xmas_roads : roads;
        return campaign_roads.roads[active_road_record];
    }

    bool is_final_unfinished_level(unsigned level) const {
        unsigned completed = 0;
        const auto count = available_level_count();
        for (std::size_t index = 0; index < count; ++index) {
            if (completion_count[index] != 0) ++completed;
        }
        return level < count && completion_count[level] == 0 &&
            completed + 1u == count;
    }

    std::size_t available_level_count() const {
        return xmas_available ? kCombinedLevelCount : kOriginalLevelCount;
    }

    void start_level(unsigned level) {
        if (level >= available_level_count()) return;
        const bool xmas_campaign = level >= kOriginalLevelCount;
        const unsigned campaign_level = xmas_campaign
            ? level - kOriginalLevelCount : level;
        const auto& campaign_roads = xmas_campaign ? xmas_roads : roads;
        if (campaign_level + 1u >= campaign_roads.road_count) return;
        road_index = level;
        auto track = static_cast<std::uint16_t>(irq_count % 12u);
        if (track == last_random_track) track = static_cast<std::uint16_t>((track + 1u) % 12u);
        last_random_track = track;
        sr_opl_load_track(&opl, &music, track + 2u, sound_disabled != 0);
        start_road(
            campaign_level + 1u, campaign_level / 3u, NativeScreen::Playing,
            is_final_unfinished_level(level), xmas_campaign);
    }

    void start_custom_level(std::size_t index) {
        if (index >= custom_levels.size()) return;
        custom_editor_index = index;
        const auto& level = custom_levels[index];
        const auto theme_count = xmas_available ? 20u : 10u;
        const auto theme = static_cast<unsigned>(level.theme % theme_count);
        const bool xmas_campaign = theme >= 10u;
        const auto world_index = theme % 10u;
        const auto& campaign_roads = xmas_campaign ? xmas_roads : roads;
        if (campaign_roads.road_count == 0u) return;
        const auto palette_road = std::min<std::size_t>(
            world_index * 3u + 1u, campaign_roads.road_count - 1u);
        const auto& palette_source = campaign_roads.roads[palette_road];

        custom_play_cells = level.cells;
        custom_play_road = {};
        custom_play_road.gravity = level.gravity;
        custom_play_road.fuel = level.fuel;
        custom_play_road.oxygen = level.oxygen;
        std::memcpy(
            custom_play_road.palette, palette_source.palette,
            sizeof(custom_play_road.palette));
        custom_play_road.cells = custom_play_cells.data();
        custom_play_road.row_count = level.row_count();
        active_custom_playtest = true;

        auto track = static_cast<std::uint16_t>(irq_count % 12u);
        if (track == last_random_track) {
            track = static_cast<std::uint16_t>((track + 1u) % 12u);
        }
        last_random_track = track;
        sr_opl_load_track(&opl, &music, track + 2u, sound_disabled != 0);
        begin_road(
            custom_play_road, world_index, NativeScreen::Playing,
            false, xmas_campaign);
    }

    void start_demo() {
        road_index = 0;
        demo_input = {};
        demo_input.mode = SR_INPUT_DEMO;
        demo_input.demo_record = demo_record.data();
        demo_input.demo_record_size = demo_record.size();
        start_road(
            0, 0, NativeScreen::Demo,
            is_final_unfinished_level(static_cast<unsigned>(road_index)), false);
    }

    void render_gameplay() {
        const auto& road = current_road();
        SrRoadFrameParams params{};
        SrDashboardInput dashboard_input{};
        SrDashboardHooks dashboard_hooks{};
        dashboard_hooks.context = this;
        dashboard_hooks.play_sound = gameplay_sound;
        require_file_load(
            sr_restore_road_viewport_vga(indexed.data(), gameplay_background.data()) != 0,
            "VGA road viewport restore");
        sr_prepare_road_frame(
            road.cells, road.row_count, &gameplay, tick_count,
            gameplay.on_kind_2, &params);
        last_road_params = params;
        if (sr_draw_road_scene_vga(
                &trek, road.cells, road.row_count, &params, &cars,
                &renderer_tables, &renderer_state, gameplay_background.data(),
                indexed.data()) == 0) {
            std::ostringstream message;
            message << "Could not decode original VGA road frame"
                    << " (record=" << active_road_record
                    << ", tick=" << tick_count
                    << ", distance=" << gameplay.position.distance
                    << ", height=" << gameplay.position.height
                    << ", ship-frame=" << params.ship_frame
                    << ", clearance=" << params.surface_clearance_units
                    << ", failure-stage=" << renderer_state.failure_stage
                    << ", depth=" << renderer_state.failure_depth
                    << ", row=" << renderer_state.failure_row
                    << ", column=" << renderer_state.failure_column
                    << ", detail=" << renderer_state.failure_detail
                    << ", offset=" << renderer_state.failure_offset << ')';
            throw std::runtime_error(message.str());
        }
        dashboard_input.tick_count = tick_count;
        dashboard_input.forward_speed = gameplay.forward_speed;
        dashboard_input.collision_speed_correction = gameplay.collision_speed_correction;
        dashboard_input.oxygen = gameplay.oxygen;
        dashboard_input.fuel = gameplay.fuel;
        dashboard_input.level_result = gameplay.level_result;
        dashboard_input.road_length_rows = gameplay_config.road_length_rows;
        dashboard_input.jumpmaster = gameplay.collision_adjusted;
        dashboard_input.distance = gameplay.position.distance;
        require_file_load(
            sr_update_gameplay_dashboard_vga(
                indexed.data(), &speed_display, &oxygen_display, &fuel_display,
                &hud, &dashboard_input, &dashboard_hooks, &dashboard_state) != 0,
            "gameplay dashboard update");
        if (cheat_status_ticks != 0u && !cheat_status.empty()) {
            draw_native_text(indexed, 4u, 4u, cheat_status, 99u);
        }
        present(active_palette);
    }

    void draw_level_completion_text() {
        const bool final_level = run_level_state.final_unfinished_level != 0;
        require_file_load(
            sr_draw_bios_text_vga(
                indexed.data(),
                static_cast<std::uint16_t>(final_level ? 0x84 : 0x68),
                0x50,
                final_level ? "The End" : "Road Completed",
                99) != 0,
            "BIOS completion text");
        present(active_palette);
    }

    void begin_level_exit(std::uint16_t result) {
        finish_coast_active = false;
        const auto step = sr_run_level_finish_gameplay(&run_level_state, result);
        active_screen = NativeScreen::LevelResult;
        if ((step.events & SR_RUN_LEVEL_EVENT_DRAW_COMPLETION) != 0) {
            draw_level_completion_text();
        }
    }

    void route_finished_level() {
        const auto result = run_level_state.result;
        const bool demo = run_level_play_screen == NativeScreen::Demo;
        if (demo) {
            if (result == SR_LEVEL_ABORTED) enter_main_menu(true);
            else start_intro();
            return;
        }
        if (active_custom_playtest) {
            active_custom_playtest = false;
            enter_custom_editor(custom_editor_index);
            return;
        }
        if (result == SR_LEVEL_COMPLETE) {
            ++completion_count[road_index];
            ++road_index;
            save_config_file();
            enter_level_selection();
        }
        else if (result == SR_LEVEL_ABORTED) {
            enter_level_selection();
        }
        else {
            const auto campaign_level = active_xmas_campaign
                ? road_index - kOriginalLevelCount : road_index;
            start_road(
                active_road_record, campaign_level / 3u, NativeScreen::Playing,
                is_final_unfinished_level(static_cast<unsigned>(road_index)),
                active_xmas_campaign);
        }
    }

    void run_level_tick() {
        const auto prior_phase = run_level_state.phase;
        const auto step = sr_run_level_tick(&run_level_state);
        std::array<std::uint8_t, kPaletteSize> palette{};
        if (prior_phase == SR_RUN_LEVEL_FADE_IN) {
            blend_full_palette(
                palette, black_palette, gameplay_palette,
                step.palette_percent);
            present(palette);
        }
        else if (prior_phase == SR_RUN_LEVEL_FADE_OUT) {
            blend_full_palette(
                palette, gameplay_palette, black_palette,
                step.palette_percent);
            present(palette);
        }
        if ((step.events & SR_RUN_LEVEL_EVENT_START_GAMEPLAY) != 0) {
            active_screen = run_level_play_screen;
        }
        if ((step.events & SR_RUN_LEVEL_EVENT_FINISHED) != 0) {
            route_finished_level();
        }
    }

    void gameplay_tick(const NativeInput& input) {
        const bool demo = active_screen == NativeScreen::Demo;
        if (active_screen != NativeScreen::Playing && !demo) return;
        if (finish_coast_active) {
            const bool coast_finished =
                sr_gameplay_finish_tick(&gameplay, &tick_count) != 0;
            sr_sound_effect_set_tick(&sound_state, tick_count);
            if (coast_finished) {
                begin_level_exit(SR_LEVEL_COMPLETE);
                return;
            }
            if (input.escape_pressed) {
                begin_level_exit(SR_LEVEL_ABORTED);
                return;
            }
            render_gameplay();
            return;
        }
        // The DOS loop presents the current state before polling input and
        // advancing physics.  In particular, it does not draw the terminal
        // state after gameplay reports a result.
        render_gameplay();
        ++tick_count;
        if (input.escape_pressed) {
            begin_level_exit(SR_LEVEL_ABORTED);
            return;
        }
        const auto& road = current_road();
        SrGameplayControls controls{};
        if (demo) {
            demo_input.road_distance = gameplay.position.distance;
            if (!sr_sample_level_input(&demo_input, nullptr)) {
                start_intro();
                return;
            }
            controls.steering = demo_input.steering;
            controls.throttle = demo_input.throttle;
            controls.jump = demo_input.jump;
        }
        else {
            live_input.mode = static_cast<SrInputMode>(selected_input_mode);
            live_input.key_flags[0] = static_cast<std::uint8_t>(input.up ? 0x80 : 0);
            live_input.key_flags[1] = static_cast<std::uint8_t>(input.down ? 0x80 : 0);
            live_input.key_flags[2] = static_cast<std::uint8_t>(input.left ? 0x80 : 0);
            live_input.key_flags[3] = static_cast<std::uint8_t>(input.right ? 0x80 : 0);
            live_input.key_flags[9] = static_cast<std::uint8_t>(input.jump ? 0x80 : 0);
            SrInputDevices devices{};
            devices.context = const_cast<NativeInput*>(&input);
            devices.joystick_axis = native_joystick_axis;
            devices.joystick_button = native_joystick_button;
            devices.mouse_value = native_mouse_value;
            devices.mouse_set_position = native_mouse_set_position;
            if (sr_sample_level_input(&live_input, &devices)) {
                controls.steering = live_input.steering;
                controls.throttle = live_input.throttle;
                controls.jump = live_input.jump;
            }
        }
        const bool jump_control = controls.jump != 0u;
        if (!demo && air_jump_cheat && jump_control && !previous_jump_control &&
            gameplay.level_result == 0u) {
            gameplay.vertical_velocity = 0x0480;
            gameplay.jumping = 1u;
            gameplay.jump_start_height = gameplay.position.height;
            gameplay.prediction_already_run = 0u;
        }
        if (!demo && air_jump_cheat && gameplay.jumping != 0u &&
            gameplay.level_result == 0u) {
            /* Native Ctrl-F12 extension: unlike the DOS rule, steering may be
               reversed or stopped at any point in the jump. */
            gameplay.lateral_velocity = static_cast<std::int16_t>(
                controls.steering * 0x1d);
        }
        previous_jump_control = jump_control;
        SrGameplayHooks hooks{};
        hooks.context = this;
        hooks.play_sound = gameplay_sound;
        hooks.sound_is_active = gameplay_sound_active;
        sr_sound_effect_set_tick(&sound_state, tick_count);
        const auto result = sr_gameplay_tick(
            road.cells, road.row_count, &gameplay_config, &controls, &hooks, &gameplay);
        if (result == SR_GAMEPLAY_TICK_FINISHED) {
            finish_coast_active = true;
            sr_gameplay_begin_finish(&gameplay, &tick_count);
            /* 1000:0E58 draws the first height-zero tube frame immediately,
               then waits for the first of its 72 synchronized coast ticks. */
            render_gameplay();
            return;
        }
        else if (sr_gameplay_result_ready(&gameplay)) {
            begin_level_exit(gameplay.level_result);
            return;
        }
    }

    void handle_custom_browser_input(const NativeInput& input) {
        if (!input.up && !input.down && !input.enter_pressed &&
            !input.escape_pressed && !input.editor_play_pressed &&
            !input.editor_page_up_pressed && !input.editor_page_down_pressed) return;
        const auto total_items = custom_browser_total_items();
        if (input.escape_pressed) {
            if (custom_browser_original_levels) {
                custom_browser_original_levels = false;
                custom_browser_selection = 1u;
                enter_custom_browser();
                return;
            }
            active_screen = NativeScreen::MainMenu;
            main_selection = 3u;
            render_main_menu();
            return;
        }
        if (input.up && custom_browser_selection != 0u) {
            --custom_browser_selection;
        }
        else if (input.down && custom_browser_selection + 1u < total_items) {
            ++custom_browser_selection;
        }
        else if (input.editor_page_up_pressed) {
            custom_browser_selection = custom_browser_selection > 13u
                ? custom_browser_selection - 13u : 0u;
        }
        else if (input.editor_page_down_pressed) {
            custom_browser_selection = std::min(
                custom_browser_selection + 13u, total_items - 1u);
        }
        else if (input.editor_play_pressed && custom_browser_selection_is_level()) {
            start_custom_level(custom_browser_level_index());
            return;
        }
        else if (input.enter_pressed) {
            if (custom_browser_selection_is_level()) {
                enter_custom_editor(custom_browser_level_index());
            }
            else if (custom_browser_original_levels) {
                custom_browser_original_levels = false;
                custom_browser_selection = 1u;
                enter_custom_browser();
            }
            else if (custom_browser_selection == 0u) create_custom_level();
            else {
                custom_browser_original_levels = true;
                custom_browser_selection = 0u;
                enter_custom_browser();
            }
            return;
        }
        render_custom_browser();
    }

    void handle_custom_editor_input(const NativeInput& input) {
        if (!input.left && !input.right && !input.up && !input.down &&
            !input.enter_pressed && !input.escape_pressed &&
            !input.editor_space_pressed && !input.editor_shape_pressed &&
            !input.editor_save_pressed && !input.editor_play_pressed &&
            !input.editor_theme_pressed && !input.editor_gravity_pressed &&
            !input.editor_fuel_pressed && !input.editor_oxygen_pressed &&
            !input.editor_insert_pressed && !input.editor_mouse_pressed &&
            !input.editor_view_pressed &&
            input.editor_material_shortcut < 0 &&
            !input.editor_delete_pressed && !input.editor_page_up_pressed &&
            !input.editor_page_down_pressed) return;
        if (custom_editor_index >= custom_levels.size()) {
            enter_custom_browser();
            return;
        }
        auto& level = custom_levels[custom_editor_index];
        const auto paint_selected = [&] {
            level.cells[
                custom_editor_row * kCustomRoadColumns + custom_editor_column] =
                static_cast<std::uint16_t>(
                    (custom_editor_brush_shape << 8u) |
                    custom_editor_brush_material);
            custom_editor_dirty = true;
        };
        const auto cycle_view = [&] {
            custom_editor_view = next_editor_view(custom_editor_view);
            custom_editor_status = std::string(editor_view_name(custom_editor_view));
        };
        if (input.escape_pressed) {
            if (custom_editor_dirty) save_custom_editor();
            custom_browser_selection =
                custom_editor_index + custom_browser_level_offset();
            enter_custom_browser();
            return;
        }
        if (input.editor_save_pressed ||
            (input.enter_pressed && !input.editor_space_pressed)) {
            save_custom_editor();
        }
        else if (input.editor_play_pressed) {
            save_custom_editor();
            if (!custom_editor_dirty) start_custom_level(custom_editor_index);
            return;
        }
        else if (input.editor_view_pressed) {
            cycle_view();
        }
        else if (input.editor_material_shortcut >= 0 &&
            static_cast<std::size_t>(input.editor_material_shortcut) <
                kEditorMaterials.size()) {
            custom_editor_brush_material = kEditorMaterials[
                static_cast<std::size_t>(input.editor_material_shortcut)];
        }
        else if (input.editor_mouse_pressed) {
            constexpr unsigned grid_x = 24u;
            constexpr unsigned grid_y = 31u;
            constexpr unsigned tile_stride_x = 25u;
            constexpr unsigned tile_stride_y = 10u;
            constexpr std::size_t visible_rows = 15u;
            const auto mouse_x = static_cast<unsigned>(input.mouse_x);
            const auto mouse_y = static_cast<unsigned>(input.mouse_y);
            if (mouse_x < 203u &&
                ((mouse_y >= 22u && mouse_y < 31u) || mouse_y >= 184u)) {
                cycle_view();
            }
            else if (custom_editor_view == EditorViewMode::Top &&
                mouse_x >= grid_x && mouse_x < grid_x + 7u * tile_stride_x &&
                mouse_y >= grid_y && mouse_y < grid_y + visible_rows * tile_stride_y) {
                const auto relative_x = mouse_x - grid_x;
                const auto relative_y = mouse_y - grid_y;
                const auto column = relative_x / tile_stride_x;
                const auto visible = relative_y / tile_stride_y;
                const auto first_row =
                    (custom_editor_row / visible_rows) * visible_rows;
                const auto row = first_row + visible;
                if (relative_x % tile_stride_x < 23u &&
                    relative_y % tile_stride_y < 9u && row < level.row_count()) {
                    custom_editor_column = column;
                    custom_editor_row = row;
                    paint_selected();
                }
            }
            else if (mouse_x >= 207u && mouse_x < 320u &&
                mouse_y >= 31u && mouse_y < 31u + 10u * 11u) {
                const auto slot = (mouse_y - 31u) / 11u;
                if ((mouse_y - 31u) % 11u < 9u && slot < kEditorMaterials.size()) {
                    custom_editor_brush_material = kEditorMaterials[slot];
                }
            }
        }
        else if (input.left && custom_editor_column != 0u) {
            --custom_editor_column;
        }
        else if (input.right && custom_editor_column + 1u < kCustomRoadColumns) {
            ++custom_editor_column;
        }
        else if (input.up && custom_editor_row != 0u) {
            --custom_editor_row;
        }
        else if (input.down && custom_editor_row + 1u < level.row_count()) {
            ++custom_editor_row;
        }
        else if (input.editor_page_up_pressed) {
            custom_editor_row = custom_editor_row > 15u
                ? custom_editor_row - 15u : 0u;
        }
        else if (input.editor_page_down_pressed) {
            custom_editor_row = std::min(
                custom_editor_row + 15u, level.row_count() - 1u);
        }
        else if (input.editor_space_pressed) {
            paint_selected();
        }
        else if (input.editor_shape_pressed) {
            custom_editor_brush_shape = static_cast<std::uint16_t>(
                (custom_editor_brush_shape + 1u) % 6u);
        }
        else if (input.editor_theme_pressed) {
            const auto theme_count = xmas_available ? 20u : 10u;
            level.theme = static_cast<std::uint16_t>((level.theme + 1u) % theme_count);
            custom_editor_dirty = true;
        }
        else if (input.editor_gravity_pressed) {
            level.gravity = static_cast<std::uint16_t>(
                level.gravity >= 20u ? 4u : level.gravity + 2u);
            custom_editor_dirty = true;
        }
        else if (input.editor_fuel_pressed) {
            level.fuel = static_cast<std::uint16_t>(
                level.fuel >= 400u ? 25u : std::max<unsigned>(25u, level.fuel + 25u));
            custom_editor_dirty = true;
        }
        else if (input.editor_oxygen_pressed) {
            level.oxygen = static_cast<std::uint16_t>(
                level.oxygen >= 400u ? 25u : std::max<unsigned>(25u, level.oxygen + 25u));
            custom_editor_dirty = true;
        }
        else if (input.editor_insert_pressed &&
            level.row_count() < kCustomRoadMaximumRows) {
            std::array<std::uint16_t, kCustomRoadColumns> copy{};
            std::copy_n(
                level.cells.begin() + static_cast<std::ptrdiff_t>(
                    custom_editor_row * kCustomRoadColumns),
                kCustomRoadColumns, copy.begin());
            level.cells.insert(
                level.cells.begin() + static_cast<std::ptrdiff_t>(
                    (custom_editor_row + 1u) * kCustomRoadColumns),
                copy.begin(), copy.end());
            ++custom_editor_row;
            custom_editor_dirty = true;
        }
        else if (input.editor_delete_pressed &&
            level.row_count() > kCustomRoadMinimumRows) {
            const auto first = level.cells.begin() + static_cast<std::ptrdiff_t>(
                custom_editor_row * kCustomRoadColumns);
            level.cells.erase(first, first + static_cast<std::ptrdiff_t>(
                kCustomRoadColumns));
            custom_editor_row = std::min(
                custom_editor_row, level.row_count() - 1u);
            custom_editor_dirty = true;
        }
        render_custom_editor();
    }

    void show_cheat_status(std::string message) {
        cheat_status = std::move(message);
        cheat_status_ticks = 360u;
        render_gameplay();
    }

    void handle_native_cheats(const NativeInput& input) {
        if (active_screen != NativeScreen::Playing) return;
        if (input.cheat_air_jump_pressed) {
            air_jump_cheat = !air_jump_cheat;
            show_cheat_status(
                air_jump_cheat ? "AIR JUMP STEER ON" : "AIR JUMP STEER OFF");
        }
        if (input.cheat_refill_pressed) {
            gameplay.fuel = 30000u;
            gameplay.oxygen = 30000u;
            if (gameplay.level_result == SR_LEVEL_OUT_OF_FUEL ||
                gameplay.level_result == SR_LEVEL_OUT_OF_OXYGEN) {
                gameplay.level_result = 0u;
                gameplay.result_delay_ticks = 0u;
            }
            show_cheat_status("FUEL OXYGEN FULL");
        }
        if (input.cheat_no_gravity_pressed) {
            no_gravity_cheat = !no_gravity_cheat;
            gameplay.gravity_step = no_gravity_cheat
                ? 0 : sr_gravity_step(gameplay_config.road_gravity);
            show_cheat_status(
                no_gravity_cheat ? "ZERO GRAVITY ON" : "ZERO GRAVITY OFF");
        }
        if (input.cheat_overdrive_pressed) {
            overdrive_cheat = !overdrive_cheat;
            gameplay_config.forward_speed_limit = overdrive_cheat
                ? kOverdriveForwardSpeedLimit : kOriginalForwardSpeedLimit;
            if (!overdrive_cheat &&
                gameplay.forward_speed > kOriginalForwardSpeedLimit) {
                gameplay.forward_speed = kOriginalForwardSpeedLimit;
            }
            show_cheat_status(
                overdrive_cheat ? "SPEED LIMIT 200" : "SPEED LIMIT 100");
        }
    }

    void dispatch_menu_input(const NativeInput& input) {
        if (active_screen == NativeScreen::CustomLevelBrowser) {
            handle_custom_browser_input(input);
            return;
        }
        if (active_screen == NativeScreen::CustomLevelEditor) {
            handle_custom_editor_input(input);
            return;
        }
        const auto key = menu_key(input);
        if (active_screen == NativeScreen::MainMenu && key != 0) {
            if (key == SR_MENU_KEY_DOWN && main_selection < 3u) {
                ++main_selection;
                if (main_selection < SR_MAIN_MENU_ITEM_COUNT) {
                    main_state.selection = static_cast<std::uint16_t>(main_selection);
                }
                render_main_menu();
                return;
            }
            if (key == SR_MENU_KEY_UP && main_selection != 0u) {
                --main_selection;
                main_state.selection = static_cast<std::uint16_t>(main_selection);
                render_main_menu();
                return;
            }
            if (main_selection == 3u &&
                (key == SR_MENU_KEY_ENTER || key == SR_MENU_KEY_ESCAPE)) {
                custom_browser_original_levels = false;
                custom_browser_selection = 0u;
                enter_custom_browser();
                return;
            }
            main_state.selection = static_cast<std::uint16_t>(main_selection);
            const auto event = sr_main_menu_key(&main_state, key);
            main_selection = main_state.selection;
            if (event.action != SR_MAIN_MENU_NONE) {
                sr_menu_flow_main_action(&menu_flow, event.action);
            }
            else render_main_menu();
        }
        else if (active_screen == NativeScreen::Settings && key != 0) {
            if (settings_selection == 5u) {
                if (key == SR_MENU_KEY_ESCAPE) {
                    save_config_file();
                    sr_menu_flow_settings_exit(&menu_flow);
                }
                else {
                    if (key == SR_MENU_KEY_UP) {
                        settings_selection = 3u;
                        settings_state.selection = 3u;
                    }
                    else if (key == SR_MENU_KEY_ENTER) {
                        high_definition = !high_definition;
                        save_native_config_file();
                    }
                    render_settings();
                }
                return;
            }
            if (key == SR_MENU_KEY_DOWN && settings_selection >= 3u) {
                settings_selection = kNativeSettingsItemCount - 1u;
                render_settings();
                return;
            }
            const auto event = sr_settings_menu_key(&settings_state, key);
            settings_selection = settings_state.selection;
            selected_input_mode = settings_state.selected_input_mode;
            sound_disabled = settings_state.sound_disabled;
            if (event.music_action == SR_SETTINGS_MUSIC_SILENCE) {
                sr_opl_disable_music(&opl);
            }
            else if (event.music_action == SR_SETTINGS_MUSIC_LOAD_TRACK_ONE) {
                sr_opl_load_track(&opl, &music, 1, 0);
            }
            if (event.action == SR_SETTINGS_MENU_EXIT) {
                save_config_file();
                sr_menu_flow_settings_exit(&menu_flow);
            }
            else render_settings();
        }
        else if (active_screen == NativeScreen::Help && key != 0) {
            sr_menu_flow_help_key(&menu_flow, key);
        }
        else if (active_screen == NativeScreen::LevelSelection && key != 0) {
            const auto action = xmas_available
                ? expanded_level_menu_key(
                    level_state, key,
                    static_cast<unsigned>(available_level_count()))
                : sr_level_menu_key(&level_state, key);
            road_index = std::min<std::size_t>(
                level_state.current_level, available_level_count() - 1u);
            if (action != SR_LEVEL_MENU_NONE) {
                sr_menu_flow_level_action(&menu_flow, action);
            }
            else render_level_menu();
        }
    }

    void timer_tick(const NativeInput& input) {
        latest_input = input;
        ++irq_count;
        sr_opl_tick(&opl);
        handle_native_cheats(input);
        if (active_screen == NativeScreen::Intro) {
            if (irq_phase == 0 || irq_phase == 5) intro_tick(input);
        }
        else if (active_screen == NativeScreen::LevelTransition ||
            active_screen == NativeScreen::LevelResult) {
            if (irq_phase == 0 || irq_phase == 5) run_level_tick();
        }
        else if (active_screen == NativeScreen::Playing ||
            active_screen == NativeScreen::Demo) {
            if (irq_phase == 0 || irq_phase == 5) {
                gameplay_tick(input);
            }
        }
        else {
            if (sr_menu_flow_accepts_input(&menu_flow)) {
                dispatch_menu_input(input);
            }
            else if (irq_phase == 0 || irq_phase == 5) {
                menu_transition_tick();
            }
        }
        if (cheat_status_ticks != 0u) --cheat_status_ticks;
        irq_phase = irq_phase == 0 ? 9 : static_cast<std::uint8_t>(irq_phase - 1u);
    }
};

RecoveredGame::RecoveredGame(const std::filesystem::path& data_root)
    : impl_(std::make_unique<Impl>(data_root)) {}

RecoveredGame::~RecoveredGame() = default;
RecoveredGame::RecoveredGame(RecoveredGame&&) noexcept = default;
RecoveredGame& RecoveredGame::operator=(RecoveredGame&&) noexcept = default;

void RecoveredGame::timer_tick(const NativeInput& input) {
    impl_->timer_tick(input);
}

const std::vector<std::uint32_t>& RecoveredGame::pixels() const {
    return impl_->rgba;
}

const std::vector<std::uint8_t>& RecoveredGame::indexed_pixels() const {
    return impl_->indexed;
}

NativeScreen RecoveredGame::screen() const {
    return impl_->active_screen;
}

std::size_t RecoveredGame::level_index() const {
    return impl_->road_index;
}

std::size_t RecoveredGame::level_count() const {
    return impl_->available_level_count();
}

bool RecoveredGame::has_xmas_levels() const {
    return impl_->xmas_available;
}

std::size_t RecoveredGame::custom_level_count() const {
    return impl_->custom_levels.size();
}

std::uint32_t RecoveredGame::road_distance() const {
    return impl_->gameplay.position.distance;
}

std::uint16_t RecoveredGame::gameplay_ticks() const {
    return impl_->tick_count;
}

std::uint16_t RecoveredGame::last_ship_frame() const {
    return impl_->last_road_params.ship_frame;
}

std::uint16_t RecoveredGame::selected_input_mode() const {
    return impl_->selected_input_mode;
}

bool RecoveredGame::high_definition_enabled() const {
    return impl_->high_definition;
}

bool RecoveredGame::air_jump_enabled() const {
    return impl_->air_jump_cheat;
}

bool RecoveredGame::no_gravity_enabled() const {
    return impl_->no_gravity_cheat;
}

bool RecoveredGame::overdrive_enabled() const {
    return impl_->overdrive_cheat;
}

std::uint16_t RecoveredGame::fuel() const {
    return impl_->gameplay.fuel;
}

std::uint16_t RecoveredGame::oxygen() const {
    return impl_->gameplay.oxygen;
}

std::int16_t RecoveredGame::vertical_velocity() const {
    return impl_->gameplay.vertical_velocity;
}

std::int16_t RecoveredGame::lateral_velocity() const {
    return impl_->gameplay.lateral_velocity;
}

std::int16_t RecoveredGame::gravity_step() const {
    return impl_->gameplay.gravity_step;
}

std::int32_t RecoveredGame::forward_speed() const {
    return impl_->gameplay.forward_speed;
}

std::int32_t RecoveredGame::forward_speed_limit() const {
    return impl_->gameplay_config.forward_speed_limit > 0
        ? impl_->gameplay_config.forward_speed_limit
        : kOriginalForwardSpeedLimit;
}

std::uint64_t RecoveredGame::presentation_revision() const {
    return impl_->presentation_revision;
}

const std::filesystem::path& RecoveredGame::data_root() const {
    return impl_->root;
}

std::vector<std::uint8_t> RecoveredGame::trek_record_bytes(std::size_t index) const {
    if (index >= impl_->trek.record_count) return {};
    const auto& record = impl_->trek.records[index];
    return {record.bytes, record.bytes + record.size};
}

std::vector<std::uint8_t> RecoveredGame::car_frame_bytes(std::size_t index) const {
    constexpr std::size_t frame_bytes = 0x02d0u;
    if (index >= impl_->cars.frame_count) return {};
    const auto* begin = impl_->cars.pixels + index * frame_bytes;
    return {begin, begin + frame_bytes};
}

std::vector<std::uint8_t> RecoveredGame::ship_mask_bytes() const {
    return {
        std::begin(impl_->renderer_state.ship_mask),
        std::end(impl_->renderer_state.ship_mask),
    };
}

void RecoveredGame::begin_palette_trace() {
    impl_->palette_trace.clear();
    impl_->palette_trace_previous = impl_->active_palette;
    impl_->palette_trace_enabled = true;
}

std::vector<std::uint8_t> RecoveredGame::consume_palette_trace() {
    auto result = std::move(impl_->palette_trace);
    impl_->palette_trace.clear();
    impl_->palette_trace_enabled = false;
    return result;
}

std::optional<PcmEffect> RecoveredGame::consume_pcm_effect() {
    if (impl_->pending_intro_sample) {
        impl_->pending_intro_sample = false;
        PcmEffect result;
        result.effect = 0xffffu;
        result.sample_rate = UINT32_C(1000000) / (256u - 0x5au);
        result.samples = impl_->intro_assets.sample;
        return result;
    }
    const auto effect = impl_->pending_effect;
    impl_->pending_effect.reset();
    if (!effect) return std::nullopt;
    SrSampleEffect sample{};
    if (!sr_get_sample_effect(&impl_->samples, *effect, &sample)) {
        return std::nullopt;
    }
    PcmEffect result;
    result.effect = *effect;
    result.sample_rate = UINT32_C(1000000) /
        (256u - sample.dsp_time_constant);
    result.samples.assign(sample.bytes, sample.bytes + sample.byte_count);
    return result;
}

std::vector<OplRegisterWrite> RecoveredGame::consume_opl_writes() {
    std::vector<OplRegisterWrite> result;
    result.swap(impl_->pending_opl_writes);
    return result;
}

} // namespace skyroads
