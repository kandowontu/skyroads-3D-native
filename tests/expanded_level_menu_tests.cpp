#include "expanded_level_menu.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

} // namespace

int main() {
    try {
        SrLevelMenuState state{};
        state.current_level = 14;
        skyroads::expanded_level_menu_key(
            state, SR_MENU_KEY_RIGHT, skyroads::kCombinedLevelCount);
        require(state.current_level == 29, "Right did not preserve the four-column row");
        skyroads::expanded_level_menu_key(
            state, SR_MENU_KEY_RIGHT, skyroads::kCombinedLevelCount);
        require(state.current_level == 44, "Right did not enter the SkyXmas columns");
        skyroads::expanded_level_menu_key(
            state, SR_MENU_KEY_RIGHT, skyroads::kCombinedLevelCount);
        require(state.current_level == 59, "Right did not reach the fourth column");
        skyroads::expanded_level_menu_key(
            state, SR_MENU_KEY_DOWN, skyroads::kCombinedLevelCount);
        require(state.current_level == 59, "Down escaped the last selector row");
        skyroads::expanded_level_menu_key(
            state, SR_MENU_KEY_LEFT, skyroads::kCombinedLevelCount);
        require(state.current_level == 44, "Left did not preserve selector row");
        require(skyroads::expanded_level_menu_key(
                state, SR_MENU_KEY_ENTER, skyroads::kCombinedLevelCount) ==
                SR_LEVEL_MENU_START,
            "Enter did not start an expanded level");

        skyroads::ExpandedLevelMenuLayout layout{};
        require(skyroads::expanded_level_menu_layout(
                0, skyroads::kCombinedLevelCount, layout) &&
                layout.selector_x == 31 && layout.selector_y == 10,
            "First expanded selector cell is misplaced");
        require(skyroads::expanded_level_menu_layout(
                30, skyroads::kCombinedLevelCount, layout) &&
                layout.selector_x == 191 && layout.selector_y == 10,
            "First SkyXmas selector cell is misplaced");
        require(skyroads::expanded_level_menu_layout(
                59, skyroads::kCombinedLevelCount, layout) &&
                layout.selector_x == 271 && layout.selector_y == 184,
            "Last SkyXmas selector cell is misplaced");

        std::vector<std::uint8_t> framebuffer(320u * 200u);
        std::vector<std::uint8_t> original_art(320u * 200u, 4);
        std::vector<std::uint8_t> xmas_art(320u * 200u, 5);
        std::array<std::uint16_t, skyroads::kCombinedLevelCount> completions{};
        completions[30] = 8;
        require(skyroads::render_expanded_level_menu(
                framebuffer, original_art.data(), xmas_art.data(),
                completions.data(), completions.size(), 30),
            "Could not render the combined selector");
        require(framebuffer[10u * 320u + 191u] == 1,
            "SkyXmas selection border was not drawn");
        require(framebuffer[12u * 320u + 231u] == 2 &&
                framebuffer[12u * 320u + 237u] == 2,
            "SkyXmas completion markers did not retain the seven-marker cap");
        require(framebuffer[10u * 320u + 2u] == 4 &&
                framebuffer[10u * 320u + 162u] == 5,
            "Original campaign thumbnails were not copied into their columns");
        require(framebuffer[9u * 320u + 1u] == 4,
            "A synthetic border was drawn around an already-bordered thumbnail");

        std::cout << "Four-column 60-level selector vectors passed\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
