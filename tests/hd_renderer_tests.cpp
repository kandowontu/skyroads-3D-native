#include "hd_renderer.hpp"

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
        const std::vector<std::uint32_t> source{
            0x000000u, 0xff0000u,
            0x00ff00u, 0x0000ffu,
        };
        std::vector<std::uint32_t> output;
        skyroads::render_smooth_quads(source, 2u, 2u, 3u, 3u, output);
        require(output.size() == 9u, "HD renderer returned the wrong output size");
        require(output[0] == source[0] && output[2] == source[1] &&
                output[6] == source[2] && output[8] == source[3],
            "HD renderer did not preserve the source corners");
        const auto center = output[4];
        require(((center >> 16u) & 0xffu) >= 63u &&
                ((center >> 8u) & 0xffu) >= 63u &&
                (center & 0xffu) >= 63u,
            "HD renderer did not smoothly shade the center quad");

        skyroads::render_smooth_quads(source, 2u, 2u, 2u, 2u, output);
        require(output == source, "HD renderer changed a one-to-one frame");
        std::cout << "HD smooth-quad renderer passed\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
