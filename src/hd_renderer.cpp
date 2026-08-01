#include "hd_renderer.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace skyroads {
namespace {

struct SampleCoordinate {
    unsigned first{};
    unsigned second{};
    unsigned fraction{};
};

std::vector<SampleCoordinate> make_coordinates(unsigned source, unsigned destination) {
    std::vector<SampleCoordinate> result(destination);
    if (source == 1u || destination == 1u) return result;
    const auto denominator = static_cast<std::uint64_t>(destination - 1u);
    for (unsigned output = 0; output < destination; ++output) {
        const auto scaled = static_cast<std::uint64_t>(output) *
            static_cast<std::uint64_t>(source - 1u) * 256u;
        const auto fixed = scaled / denominator;
        result[output].first = static_cast<unsigned>(fixed >> 8u);
        result[output].second = std::min(result[output].first + 1u, source - 1u);
        result[output].fraction = static_cast<unsigned>(fixed & 0xffu);
    }
    return result;
}

unsigned interpolate(unsigned first, unsigned second, unsigned fraction) {
    return (first * (256u - fraction) + second * fraction + 128u) >> 8u;
}

std::uint32_t interpolate_pixel(
    std::uint32_t top_left,
    std::uint32_t top_right,
    std::uint32_t bottom_left,
    std::uint32_t bottom_right,
    unsigned x_fraction,
    unsigned y_fraction) {
    std::uint32_t result = 0;
    for (unsigned shift : {0u, 8u, 16u}) {
        const auto top = interpolate(
            (top_left >> shift) & 0xffu,
            (top_right >> shift) & 0xffu,
            x_fraction);
        const auto bottom = interpolate(
            (bottom_left >> shift) & 0xffu,
            (bottom_right >> shift) & 0xffu,
            x_fraction);
        result |= interpolate(top, bottom, y_fraction) << shift;
    }
    return result;
}

} // namespace

void render_smooth_quads(
    const std::vector<std::uint32_t>& source,
    unsigned source_width,
    unsigned source_height,
    unsigned destination_width,
    unsigned destination_height,
    std::vector<std::uint32_t>& destination) {
    if (source_width == 0u || source_height == 0u ||
        destination_width == 0u || destination_height == 0u ||
        source.size() != static_cast<std::size_t>(source_width) * source_height) {
        throw std::invalid_argument("Invalid smooth-quad framebuffer dimensions");
    }
    if (source_width == destination_width && source_height == destination_height) {
        destination = source;
        return;
    }

    const auto x_coordinates = make_coordinates(source_width, destination_width);
    const auto y_coordinates = make_coordinates(source_height, destination_height);
    destination.resize(static_cast<std::size_t>(destination_width) * destination_height);
    for (unsigned y = 0; y < destination_height; ++y) {
        const auto& sy = y_coordinates[y];
        const auto top = static_cast<std::size_t>(sy.first) * source_width;
        const auto bottom = static_cast<std::size_t>(sy.second) * source_width;
        for (unsigned x = 0; x < destination_width; ++x) {
            const auto& sx = x_coordinates[x];
            destination[static_cast<std::size_t>(y) * destination_width + x] =
                interpolate_pixel(
                    source[top + sx.first], source[top + sx.second],
                    source[bottom + sx.first], source[bottom + sx.second],
                    sx.fraction, sy.fraction);
        }
    }
}

} // namespace skyroads
