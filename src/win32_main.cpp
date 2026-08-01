#include "recovered_game.hpp"
#include "hd_renderer.hpp"
#include "win32_opl_audio.hpp"

#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

using skyroads::NativeInput;
using skyroads::NativeScreen;
using skyroads::RecoveredGame;

std::unique_ptr<RecoveredGame> g_game;
std::array<bool, 256> g_held{};
std::array<bool, 256> g_pressed{};
std::filesystem::path g_data_root;
HWND g_window{};
constexpr std::uint16_t kMouseInputMode = 2;
constexpr std::uint16_t kDosJoystickAxisMaximum = 0x1770u;

struct WindowBackBuffer {
    HDC dc{};
    HBITMAP bitmap{};
    HGDIOBJ previous_bitmap{};
    int width{};
    int height{};

    ~WindowBackBuffer() { reset(); }

    void reset() {
        if (dc && previous_bitmap) SelectObject(dc, previous_bitmap);
        if (bitmap) DeleteObject(bitmap);
        if (dc) DeleteDC(dc);
        dc = nullptr;
        bitmap = nullptr;
        previous_bitmap = nullptr;
        width = 0;
        height = 0;
    }

    bool resize(HDC destination, int new_width, int new_height) {
        if (dc && width == new_width && height == new_height) return true;
        reset();
        if (new_width <= 0 || new_height <= 0) return false;
        dc = CreateCompatibleDC(destination);
        bitmap = CreateCompatibleBitmap(destination, new_width, new_height);
        if (!dc || !bitmap) {
            reset();
            return false;
        }
        previous_bitmap = SelectObject(dc, bitmap);
        if (!previous_bitmap || previous_bitmap == HGDI_ERROR) {
            previous_bitmap = nullptr;
            reset();
            return false;
        }
        width = new_width;
        height = new_height;
        return true;
    }
};

WindowBackBuffer g_back_buffer;
std::vector<std::uint32_t> g_hd_pixels;

std::uint16_t normalize_joystick_axis(
    DWORD value, UINT minimum, UINT maximum) {
    if (maximum <= minimum) return kDosJoystickAxisMaximum / 2u;
    const auto clamped = std::clamp<DWORD>(value, minimum, maximum);
    const auto range = static_cast<std::uint64_t>(maximum) - minimum;
    const auto position = static_cast<std::uint64_t>(clamped) - minimum;
    return static_cast<std::uint16_t>(
        (position * kDosJoystickAxisMaximum + range / 2u) / range);
}

std::filesystem::path executable_directory() {
    std::wstring buffer(32768, L'\0');
    const auto size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (size == 0 || size >= buffer.size()) return std::filesystem::current_path();
    buffer.resize(size);
    return std::filesystem::path(buffer).parent_path();
}

bool has_original_executable(const std::filesystem::path& path) {
    std::error_code error;
    if (!std::filesystem::is_directory(path, error)) return false;
    return
        std::filesystem::is_regular_file(path / "skyroads.exe", error) ||
        std::filesystem::is_regular_file(path / "skyxmas.exe", error);
}

std::filesystem::path locate_data_root() {
    const auto executable = executable_directory();
    if (has_original_executable(executable)) {
        return std::filesystem::absolute(executable);
    }
    throw std::runtime_error(
        "The running folder must contain an original SKYROADS.EXE or "
        "SKYXMAS.EXE.\n\nCopy either original DOS executable beside "
        "skyroads_native.exe; the required game archives are built in.");
}

NativeInput collect_input() {
    NativeInput input;
    const bool playing = g_game &&
        (g_game->screen() == NativeScreen::Playing ||
         g_game->screen() == NativeScreen::Demo ||
         g_game->screen() == NativeScreen::LevelResult);
    const auto key = [playing](unsigned value) {
        return playing ? g_held[value] : g_pressed[value];
    };
    input.left = key(VK_LEFT) || key('A') || key(VK_NUMPAD4) ||
        key(VK_NUMPAD7) || key(VK_NUMPAD1);
    input.right = key(VK_RIGHT) || key('D') || key(VK_NUMPAD6) ||
        key(VK_NUMPAD9) || key(VK_NUMPAD3);
    input.up = key(VK_UP) || key('W') || key(VK_NUMPAD8) ||
        key(VK_NUMPAD7) || key(VK_NUMPAD9);
    input.down = key(VK_DOWN) || key('S') || key(VK_NUMPAD2) ||
        key(VK_NUMPAD1) || key(VK_NUMPAD3);
    input.jump = g_held[VK_SPACE];
    input.enter_pressed = g_pressed[VK_RETURN] || g_pressed[VK_SPACE];
    input.escape_pressed = g_pressed[VK_ESCAPE];
    input.editor_space_pressed = g_pressed[VK_SPACE];
    input.editor_shape_pressed = g_pressed['T'];
    input.editor_save_pressed = g_pressed['S'];
    input.editor_play_pressed = g_pressed['P'];
    input.editor_theme_pressed = g_pressed['W'];
    input.editor_gravity_pressed = g_pressed['G'];
    input.editor_fuel_pressed = g_pressed['F'];
    input.editor_oxygen_pressed = g_pressed['O'];
    input.editor_insert_pressed = g_pressed[VK_INSERT];
    input.editor_delete_pressed = g_pressed[VK_DELETE];
    input.editor_page_up_pressed = g_pressed[VK_PRIOR];
    input.editor_page_down_pressed = g_pressed[VK_NEXT];
    input.editor_mouse_pressed = g_pressed[VK_LBUTTON];
    for (std::int8_t slot = 0; slot < 10; ++slot) {
        const auto key = static_cast<unsigned>('0' + slot);
        if (g_pressed[key]) input.editor_material_shortcut = slot;
    }
    const bool control_held = g_held[VK_CONTROL] ||
        g_held[VK_LCONTROL] || g_held[VK_RCONTROL];
    input.cheat_air_jump_pressed = control_held && g_pressed[VK_F12];
    input.cheat_refill_pressed = control_held && g_pressed[VK_F11];
    input.cheat_no_gravity_pressed = control_held && g_pressed[VK_F10];
    input.cheat_overdrive_pressed = control_held && g_pressed[VK_F9];

    JOYINFOEX joystick{};
    joystick.dwSize = sizeof(joystick);
    joystick.dwFlags = JOY_RETURNX | JOY_RETURNY | JOY_RETURNBUTTONS;
    if (joyGetPosEx(JOYSTICKID1, &joystick) == JOYERR_NOERROR) {
        JOYCAPSW capabilities{};
        input.joystick_connected = true;
        if (joyGetDevCapsW(
                JOYSTICKID1, &capabilities, sizeof(capabilities)) == JOYERR_NOERROR) {
            /* 1000:05F6 measures each DOS game-port axis with the PIT and
             * times out at 0x1770.  Preserve those units before applying the
             * recovered half/three-halves calibration thresholds. */
            input.joystick_x = normalize_joystick_axis(
                joystick.dwXpos, capabilities.wXmin, capabilities.wXmax);
            input.joystick_y = normalize_joystick_axis(
                joystick.dwYpos, capabilities.wYmin, capabilities.wYmax);
        }
        else {
            input.joystick_x = normalize_joystick_axis(joystick.dwXpos, 0, 0xffffu);
            input.joystick_y = normalize_joystick_axis(joystick.dwYpos, 0, 0xffffu);
        }
        input.joystick_button = joystick.dwButtons != 0;
    }

    if (g_window) {
        POINT cursor{};
        RECT client{};
        if (GetCursorPos(&cursor) && ScreenToClient(g_window, &cursor) &&
            GetClientRect(g_window, &client)) {
            const int client_width = std::max(1L, client.right - client.left);
            const int client_height = std::max(1L, client.bottom - client.top);
            int width = client_width;
            int height = width * skyroads::kScreenHeight / skyroads::kScreenWidth;
            if (height > client_height) {
                height = client_height;
                width = height * skyroads::kScreenWidth / skyroads::kScreenHeight;
            }
            const int x = (client_width - width) / 2;
            const int y = (client_height - height) / 2;
            input.mouse_available = true;
            input.mouse_x = static_cast<std::uint16_t>(std::clamp<int>(
                (cursor.x - x) * skyroads::kScreenWidth / std::max(1, width),
                0, skyroads::kScreenWidth - 1));
            input.mouse_y = static_cast<std::uint16_t>(std::clamp<int>(
                (cursor.y - y) * skyroads::kScreenHeight / std::max(1, height),
                0, skyroads::kScreenHeight - 1));
            input.mouse_button = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        }
    }
    return input;
}

void apply_original_mouse_recenter() {
    if (!g_window || !g_game ||
        g_game->selected_input_mode() != kMouseInputMode ||
        g_game->screen() != NativeScreen::Playing) return;
    POINT cursor{};
    RECT client{};
    if (!GetCursorPos(&cursor) || !ScreenToClient(g_window, &cursor) ||
        !GetClientRect(g_window, &client)) return;
    cursor.x = (client.right - client.left) / 2;
    if (ClientToScreen(g_window, &cursor)) SetCursorPos(cursor.x, cursor.y);
}

void paint(HWND window) {
    PAINTSTRUCT paint{};
    HDC dc = BeginPaint(window, &paint);
    RECT client{};
    GetClientRect(window, &client);
    const int client_width = client.right - client.left;
    const int client_height = client.bottom - client.top;
    if (client_width <= 0 || client_height <= 0 || !g_game) {
        EndPaint(window, &paint);
        return;
    }
    int width = client_width;
    int height = width * skyroads::kScreenHeight / skyroads::kScreenWidth;
    if (height > client_height) {
        height = client_height;
        width = height * skyroads::kScreenWidth / skyroads::kScreenHeight;
    }
    const int x = (client_width - width) / 2;
    const int y = (client_height - height) / 2;

    unsigned bitmap_width = skyroads::kScreenWidth;
    unsigned bitmap_height = skyroads::kScreenHeight;
    const std::uint32_t* bitmap_pixels = g_game->pixels().data();
    if (g_game->high_definition_enabled()) {
        skyroads::render_smooth_quads(
            g_game->pixels(), skyroads::kScreenWidth, skyroads::kScreenHeight,
            static_cast<unsigned>(width), static_cast<unsigned>(height),
            g_hd_pixels);
        bitmap_width = static_cast<unsigned>(width);
        bitmap_height = static_cast<unsigned>(height);
        bitmap_pixels = g_hd_pixels.data();
    }

    BITMAPINFO bitmap{};
    bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmap.bmiHeader.biWidth = static_cast<LONG>(bitmap_width);
    bitmap.bmiHeader.biHeight = -static_cast<LONG>(bitmap_height);
    bitmap.bmiHeader.biPlanes = 1;
    bitmap.bmiHeader.biBitCount = 32;
    bitmap.bmiHeader.biCompression = BI_RGB;
    HDC target = dc;
    if (g_back_buffer.resize(dc, client_width, client_height)) {
        target = g_back_buffer.dc;
    }
    FillRect(target, &client, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    SetStretchBltMode(target, COLORONCOLOR);
    StretchDIBits(target, x, y, width, height, 0, 0,
        static_cast<int>(bitmap_width), static_cast<int>(bitmap_height),
        bitmap_pixels, &bitmap, DIB_RGB_COLORS, SRCCOPY);
    if (target != dc) {
        BitBlt(dc, 0, 0, client_width, client_height, target, 0, 0, SRCCOPY);
    }
    EndPaint(window, &paint);
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_ERASEBKGND:
        return 1;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        const auto key = static_cast<unsigned>(wparam);
        if (key < g_held.size()) {
            if (!g_held[key]) g_pressed[key] = true;
            g_held[key] = true;
        }
        return 0;
    }
    case WM_KEYUP:
    case WM_SYSKEYUP: {
        const auto key = static_cast<unsigned>(wparam);
        if (key < g_held.size()) g_held[key] = false;
        return 0;
    }
    case WM_LBUTTONDOWN:
        if (!g_held[VK_LBUTTON]) g_pressed[VK_LBUTTON] = true;
        g_held[VK_LBUTTON] = true;
        SetCapture(window);
        return 0;
    case WM_LBUTTONUP:
        g_held[VK_LBUTTON] = false;
        if (GetCapture() == window) ReleaseCapture();
        return 0;
    case WM_KILLFOCUS:
        g_held.fill(false);
        g_pressed.fill(false);
        return 0;
    case WM_PAINT:
        paint(window);
        return 0;
    case WM_DESTROY:
        g_back_buffer.reset();
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, wparam, lparam);
    }
}

} // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show_command) {
    SetProcessDPIAware();
    try {
        g_data_root = locate_data_root();
        g_game = std::make_unique<RecoveredGame>(g_data_root);
        skyroads::Win32OplAudio opl_audio;
        const auto initial_opl_writes = g_game->consume_opl_writes();
        opl_audio.write_registers(initial_opl_writes);

        constexpr wchar_t class_name[] = L"SkyRoadsNativeWindow";
        WNDCLASSEXW window_class{};
        window_class.cbSize = sizeof(window_class);
        window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        window_class.lpfnWndProc = window_proc;
        window_class.hInstance = instance;
        window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        window_class.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
        window_class.lpszClassName = class_name;
        if (!RegisterClassExW(&window_class)) throw std::runtime_error("Unable to register the window class");

        RECT rectangle{0, 0, 960, 600};
        constexpr DWORD style = WS_OVERLAPPEDWINDOW;
        AdjustWindowRect(&rectangle, style, FALSE);
        const wchar_t* window_title = g_game->has_xmas_levels()
            ? L"SkyRoads + SkyRoads Xmas Native - Executable Reconstruction"
            : L"SkyRoads Native - Executable Reconstruction";
        HWND window = CreateWindowExW(
            0, class_name, window_title, style,
            CW_USEDEFAULT, CW_USEDEFAULT, rectangle.right - rectangle.left, rectangle.bottom - rectangle.top,
            nullptr, nullptr, instance, nullptr);
        if (!window) throw std::runtime_error("Unable to create the game window");
        g_window = window;
        ShowWindow(window, show_command);
        UpdateWindow(window);
        auto invalidated_revision = g_game->presentation_revision();

        using clock = std::chrono::steady_clock;
        constexpr double tick_seconds = 1.0 / skyroads::kDosIrqRate;
        auto previous = clock::now();
        double accumulator = 0;
        bool running = true;
        while (running) {
            MSG message{};
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                if (message.message == WM_QUIT) running = false;
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            if (!running) break;

            const auto now = clock::now();
            accumulator += std::min(0.1, std::chrono::duration<double>(now - previous).count());
            previous = now;
            bool updated = false;
            while (accumulator >= tick_seconds) {
                const auto before_game_tick = g_game->gameplay_ticks();
                g_game->timer_tick(collect_input());
                const auto opl_writes = g_game->consume_opl_writes();
                if (auto effect = g_game->consume_pcm_effect()) {
                    opl_audio.start_effect(std::move(*effect));
                }
                opl_audio.advance_irq(opl_writes);
                if (g_game->gameplay_ticks() != before_game_tick) {
                    apply_original_mouse_recenter();
                }
                g_pressed.fill(false);
                accumulator -= tick_seconds;
                updated = true;
            }
            if (updated) {
                const auto revision = g_game->presentation_revision();
                if (revision != invalidated_revision) {
                    invalidated_revision = revision;
                    InvalidateRect(window, nullptr, FALSE);
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return 0;
    } catch (const std::exception& error) {
        const std::string message = std::string("SkyRoads Native could not start.\n\n") + error.what();
        MessageBoxA(nullptr, message.c_str(), "SkyRoads Native", MB_OK | MB_ICONERROR);
        return 1;
    }
}
