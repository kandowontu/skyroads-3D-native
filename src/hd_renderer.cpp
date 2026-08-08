#include "hd_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>

namespace skyroads {
namespace {

struct Point {
    double x{};
    double y{};
};

struct SmoothSpan {
    double y{};
    double left{};
    double right{};
};

void validate_frame(
    const std::vector<std::uint32_t>& frame,
    std::size_t expected_size,
    const char* name) {
    if (frame.size() != expected_size) {
        throw std::invalid_argument(std::string("Invalid ") + name +
            " framebuffer dimensions");
    }
}

double mix(double first, double second, double amount) {
    return first + (second - first) * amount;
}

bool compatible_shape(
    const RecoveredRoadShape& current,
    const RecoveredRoadShape* previous) {
    if (previous == nullptr ||
        previous->palette_index != current.palette_index ||
        previous->spans.size() != current.spans.size() ||
        current.spans.empty()) return false;
    const auto& current_first = current.spans.front();
    const auto& current_last = current.spans.back();
    const auto& previous_first = previous->spans.front();
    const auto& previous_last = previous->spans.back();
    return std::abs(current_first.y - previous_first.y) <= 4 &&
        std::abs(current_last.y - previous_last.y) <= 4 &&
        std::abs(current_first.left - previous_first.left) <= 64 &&
        std::abs(current_first.right - previous_first.right) <= 64 &&
        std::abs(current_last.left - previous_last.left) <= 64 &&
        std::abs(current_last.right - previous_last.right) <= 64;
}

SmoothSpan smooth_span(
    const RecoveredRoadShape& current,
    const RecoveredRoadShape* previous,
    std::size_t index,
    double interpolation) {
    const auto& span = current.spans[index];
    if (!compatible_shape(current, previous)) {
        return {static_cast<double>(span.y), static_cast<double>(span.left),
            static_cast<double>(span.right)};
    }
    const auto& prior = previous->spans[index];
    return {
        mix(prior.y, span.y, interpolation),
        mix(prior.left, span.left, interpolation),
        mix(prior.right, span.right, interpolation)};
}

bool sample_shape(
    const RecoveredRoadShape& current,
    const RecoveredRoadShape* previous,
    double interpolation,
    double source_y,
    double& left,
    double& right) {
    if (current.spans.empty()) return false;
    const auto first = smooth_span(current, previous, 0u, interpolation);
    const auto last = smooth_span(
        current, previous, current.spans.size() - 1u, interpolation);
    if (source_y < first.y - 0.5 || source_y > last.y + 0.5) return false;
    if (source_y <= first.y || current.spans.size() == 1u) {
        left = first.left;
        right = first.right;
        return true;
    }
    for (std::size_t index = 1u; index < current.spans.size(); ++index) {
        const auto second = smooth_span(
            current, previous, index, interpolation);
        if (source_y <= second.y) {
            const auto prior = smooth_span(
                current, previous, index - 1u, interpolation);
            const auto height = second.y - prior.y;
            const auto amount = height <= 0.0
                ? 1.0 : std::clamp((source_y - prior.y) / height, 0.0, 1.0);
            left = mix(prior.left, second.left, amount);
            right = mix(prior.right, second.right, amount);
            return true;
        }
    }
    left = last.left;
    right = last.right;
    return true;
}

void draw_shape(
    std::vector<std::uint32_t>& destination,
    unsigned source_width,
    unsigned source_height,
    unsigned destination_width,
    unsigned destination_height,
    const RecoveredRoadShape& current,
    const RecoveredRoadShape* previous,
    double interpolation) {
    if (current.spans.empty()) return;
    const auto y_scale =
        static_cast<double>(destination_height) / source_height;
    const auto x_scale =
        static_cast<double>(destination_width) / source_width;
    const auto first = smooth_span(current, previous, 0u, interpolation);
    const auto last = smooth_span(
        current, previous, current.spans.size() - 1u, interpolation);
    const auto destination_top = std::clamp(
        static_cast<int>(std::floor((first.y - 0.5) * y_scale)),
        0, static_cast<int>(destination_height));
    const auto destination_bottom = std::clamp(
        static_cast<int>(std::ceil((last.y + 1.5) * y_scale)),
        0, static_cast<int>(destination_height));
    for (int y = destination_top; y < destination_bottom; ++y) {
        const auto source_y =
            (static_cast<double>(y) + 0.5) / y_scale - 0.5;
        double left{};
        double right{};
        if (!sample_shape(
                current, previous, interpolation, source_y, left, right)) continue;
        const auto destination_left = std::clamp(
            static_cast<int>(std::floor(left * x_scale)),
            0, static_cast<int>(destination_width));
        const auto destination_right = std::clamp(
            static_cast<int>(std::ceil(right * x_scale)),
            0, static_cast<int>(destination_width));
        const auto row = static_cast<std::size_t>(y) * destination_width;
        for (int x = destination_left; x < destination_right; ++x) {
            destination[row + static_cast<unsigned>(x)] = current.color;
        }
    }
}

void fill_polygon(
    std::vector<std::uint32_t>& destination,
    unsigned source_width,
    unsigned source_height,
    unsigned destination_width,
    unsigned destination_height,
    const std::vector<Point>& source_points,
    std::uint32_t color,
    const std::vector<std::uint8_t>* visibility_mask = nullptr) {
    if (source_points.size() < 3u) return;
    std::vector<Point> points;
    points.reserve(source_points.size());
    const auto x_scale =
        static_cast<double>(destination_width) / source_width;
    const auto y_scale =
        static_cast<double>(destination_height) / source_height;
    double top = static_cast<double>(destination_height);
    double bottom = 0.0;
    for (const auto& point : source_points) {
        points.push_back({point.x * x_scale, point.y * y_scale});
        top = std::min(top, points.back().y);
        bottom = std::max(bottom, points.back().y);
    }
    const auto first_y = std::clamp(
        static_cast<int>(std::floor(top)), 0,
        static_cast<int>(destination_height));
    const auto last_y = std::clamp(
        static_cast<int>(std::ceil(bottom)), 0,
        static_cast<int>(destination_height));
    std::vector<double> intersections;
    intersections.reserve(points.size());
    for (int y = first_y; y < last_y; ++y) {
        intersections.clear();
        const auto scan_y = static_cast<double>(y) + 0.5;
        for (std::size_t index = 0; index < points.size(); ++index) {
            const auto& first = points[index];
            const auto& second = points[(index + 1u) % points.size()];
            if ((first.y <= scan_y && second.y > scan_y) ||
                (second.y <= scan_y && first.y > scan_y)) {
                const auto amount = (scan_y - first.y) / (second.y - first.y);
                intersections.push_back(mix(first.x, second.x, amount));
            }
        }
        std::sort(intersections.begin(), intersections.end());
        for (std::size_t pair = 0; pair + 1u < intersections.size(); pair += 2u) {
            const auto left = std::clamp(
                static_cast<int>(std::floor(intersections[pair])),
                0, static_cast<int>(destination_width));
            const auto right = std::clamp(
                static_cast<int>(std::ceil(intersections[pair + 1u])),
                0, static_cast<int>(destination_width));
            const auto row = static_cast<std::size_t>(y) * destination_width;
            for (int x = left; x < right; ++x) {
                if (visibility_mask != nullptr) {
                    const auto source_x = std::min(
                        static_cast<unsigned>(
                            static_cast<std::uint64_t>(x) * source_width /
                            destination_width),
                        source_width - 1u);
                    const auto source_y = std::min(
                        static_cast<unsigned>(
                            static_cast<std::uint64_t>(y) * source_height /
                            destination_height),
                        source_height - 1u);
                    if (((*visibility_mask)[
                            static_cast<std::size_t>(source_y) * source_width +
                            source_x] & 0x02u) == 0u) {
                        continue;
                    }
                }
                destination[row + static_cast<unsigned>(x)] = color;
            }
        }
    }
}

std::vector<Point> ellipse(
    double center_x,
    double center_y,
    double radius_x,
    double radius_y,
    unsigned segments = 24u) {
    std::vector<Point> points;
    points.reserve(segments);
    for (unsigned segment = 0; segment < segments; ++segment) {
        const auto angle = std::numbers::pi * 2.0 * segment / segments;
        points.push_back({center_x + std::cos(angle) * radius_x,
            center_y + std::sin(angle) * radius_y});
    }
    return points;
}

RecoveredShipModel interpolated_ship(
    const RecoveredShipModel& current,
    const RecoveredShipModel* previous,
    double interpolation) {
    auto result = current;
    if (previous != nullptr && previous->visible && current.visible) {
        result.center_x = mix(previous->center_x, current.center_x, interpolation);
        result.center_y = mix(previous->center_y, current.center_y, interpolation);
        result.shadow_y = mix(previous->shadow_y, current.shadow_y, interpolation);
        result.yaw = mix(previous->yaw, current.yaw, interpolation);
        result.pitch = mix(previous->pitch, current.pitch, interpolation);
        result.engine_pulse = mix(
            previous->engine_pulse, current.engine_pulse, interpolation);
    }
    return result;
}

void draw_ship_model(
    std::vector<std::uint32_t>& destination,
    unsigned source_width,
    unsigned source_height,
    unsigned destination_width,
    unsigned destination_height,
    const RecoveredShipModel& current,
    const RecoveredShipModel* previous,
    double interpolation,
    const std::vector<std::uint8_t>* visibility_mask) {
    const auto ship = interpolated_ship(current, previous, interpolation);
    if (!ship.visible) return;

    if (ship.shadow_visible) {
        fill_polygon(destination, source_width, source_height,
            destination_width, destination_height,
            ellipse(ship.center_x, ship.shadow_y, 12.0, 2.4, 28u), ship.shadow);
    }

    const auto transform = [&ship](double x, double y) -> Point {
        const auto compressed_x = x * (1.0 - std::abs(ship.yaw) * 0.09);
        const auto yawed_x = compressed_x + ship.yaw * y * 0.18;
        const auto pitched_y = y + ship.pitch * (-y / 9.0) * 1.4;
        return {ship.center_x + yawed_x, ship.center_y + pitched_y};
    };
    const auto polygon = [&transform](std::initializer_list<Point> points) {
        std::vector<Point> transformed;
        transformed.reserve(points.size());
        for (const auto& point : points) {
            transformed.push_back(transform(point.x, point.y));
        }
        return transformed;
    };
    const auto pod = [&](double x, double y, double rx, double ry,
                         std::uint32_t color) {
        auto points = ellipse(x, y, rx, ry, 20u);
        for (auto& point : points) {
            point = transform(point.x, point.y);
        }
        fill_polygon(destination, source_width, source_height,
            destination_width, destination_height, points, color,
            visibility_mask);
    };

    fill_polygon(destination, source_width, source_height,
        destination_width, destination_height,
        polygon({{-14,5},{-11,0},{-6,-2},{-3,-5},{-1,-9},{2,-9},
                 {4,-4},{7,-2},{12,0},{14,5},{13,8},{7,9},{4,7},
                 {3,10},{-3,10},{-4,7},{-7,9},{-13,8}}), ship.outline,
        visibility_mask);
    fill_polygon(destination, source_width, source_height,
        destination_width, destination_height,
        polygon({{-13,5},{-9,0},{-4,-1},{-5,6},{-8,8},{-12,7}}),
        ship.middle_blue, visibility_mask);
    fill_polygon(destination, source_width, source_height,
        destination_width, destination_height,
        polygon({{13,5},{9,0},{4,-1},{5,6},{8,8},{12,7}}),
        ship.middle_blue, visibility_mask);

    pod(-8.5, 4.3, 5.1, 4.2, ship.dark_blue);
    pod(8.5, 4.3, 5.1, 4.2, ship.dark_blue);
    pod(-8.5, 4.0, 4.2, 3.3, ship.light_blue);
    pod(8.5, 4.0, 4.2, 3.3, ship.light_blue);
    const auto pulse = 1.0 + ship.engine_pulse * 0.12;
    pod(-8.5, 4.7, 3.1 * pulse, 2.3 * pulse, ship.engine_red);
    pod(8.5, 4.7, 3.1 * pulse, 2.3 * pulse, ship.engine_red);
    pod(-8.5, 4.2, 2.2 * pulse, 1.4 * pulse, ship.engine_glow);
    pod(8.5, 4.2, 2.2 * pulse, 1.4 * pulse, ship.engine_glow);

    fill_polygon(destination, source_width, source_height,
        destination_width, destination_height,
        polygon({{-4,8},{-4,-2},{-2,-6},{2,-6},{4,-2},{4,8},{1,10},{-2,10}}),
        ship.dark_blue, visibility_mask);
    fill_polygon(destination, source_width, source_height,
        destination_width, destination_height,
        polygon({{-3,6},{-3,-1},{-1,-5},{2,-4},{3,0},{3,6},{1,8},{-1,8}}),
        ship.middle_blue, visibility_mask);
    fill_polygon(destination, source_width, source_height,
        destination_width, destination_height,
        polygon({{-2,0},{-1,-4},{2,-3},{2,1},{0,3}}), ship.highlight,
        visibility_mask);
    fill_polygon(destination, source_width, source_height,
        destination_width, destination_height,
        polygon({{-1,-5},{-1,-9},{1,-11},{2,-5}}), ship.outline,
        visibility_mask);
    fill_polygon(destination, source_width, source_height,
        destination_width, destination_height,
        polygon({{0,-6},{0,-9},{1,-9},{1,-6}}), ship.light_blue,
        visibility_mask);
}

} // namespace

void render_recovered_road_polygons(
    const std::vector<std::uint32_t>& background,
    const std::vector<std::uint32_t>& road_without_ship,
    const std::vector<std::uint32_t>& complete_frame,
    const std::vector<RecoveredRoadShape>& road_shapes,
    std::size_t ship_layer,
    const RecoveredShipModel& ship,
    const std::vector<std::uint8_t>& ship_exclusion_mask,
    const std::vector<RecoveredRoadShape>* previous_road_shapes,
    const RecoveredShipModel* previous_ship,
    double interpolation,
    unsigned source_width,
    unsigned source_height,
    unsigned destination_width,
    unsigned destination_height,
    std::vector<std::uint32_t>& destination) {
    if (source_width == 0u || source_height == 0u ||
        destination_width == 0u || destination_height == 0u) {
        throw std::invalid_argument("Invalid recovered-polygon dimensions");
    }
    const auto source_size =
        static_cast<std::size_t>(source_width) * source_height;
    validate_frame(background, source_size, "background");
    validate_frame(road_without_ship, source_size, "road geometry");
    validate_frame(complete_frame, source_size, "complete");
    if (ship_exclusion_mask.size() != source_size) {
        throw std::invalid_argument("Invalid Hi-Def ship exclusion mask");
    }
    const auto has_ship_visibility = std::any_of(
        ship_exclusion_mask.begin(), ship_exclusion_mask.end(),
        [](std::uint8_t value) { return (value & 0x02u) != 0u; });
    const auto* ship_visibility_mask = has_ship_visibility
        ? &ship_exclusion_mask : nullptr;
    interpolation = std::clamp(interpolation, 0.0, 1.0);
    destination.resize(
        static_cast<std::size_t>(destination_width) * destination_height);

    for (unsigned y = 0; y < destination_height; ++y) {
        const auto source_y = std::min(
            static_cast<unsigned>(
                static_cast<std::uint64_t>(y) * source_height /
                destination_height),
            source_height - 1u);
        for (unsigned x = 0; x < destination_width; ++x) {
            const auto source_x = std::min(
                static_cast<unsigned>(
                    static_cast<std::uint64_t>(x) * source_width /
                    destination_width),
                source_width - 1u);
            destination[static_cast<std::size_t>(y) * destination_width + x] =
                background[static_cast<std::size_t>(source_y) * source_width +
                    source_x];
        }
    }

    const auto layer = std::min(ship_layer, road_shapes.size());
    for (std::size_t shape = 0; shape <= road_shapes.size(); ++shape) {
        if (shape == layer) {
            draw_ship_model(destination, source_width, source_height,
                destination_width, destination_height, ship, previous_ship,
                interpolation, ship_visibility_mask);
        }
        if (shape == road_shapes.size()) break;
        const RecoveredRoadShape* previous = nullptr;
        if (previous_road_shapes != nullptr &&
            shape < previous_road_shapes->size()) {
            previous = &(*previous_road_shapes)[shape];
        }
        draw_shape(destination, source_width, source_height,
            destination_width, destination_height, road_shapes[shape], previous,
            interpolation);
    }

    for (unsigned y = 0; y < destination_height; ++y) {
        const auto source_y = std::min(
            static_cast<unsigned>(
                static_cast<std::uint64_t>(y) * source_height /
                destination_height),
            source_height - 1u);
        for (unsigned x = 0; x < destination_width; ++x) {
            const auto source_x = std::min(
                static_cast<unsigned>(
                    static_cast<std::uint64_t>(x) * source_width /
                    destination_width),
                source_width - 1u);
            const auto source_at =
                static_cast<std::size_t>(source_y) * source_width + source_x;
            if ((ship_exclusion_mask[source_at] & 0x01u) == 0u &&
                complete_frame[source_at] != road_without_ship[source_at]) {
                destination[static_cast<std::size_t>(y) * destination_width + x] =
                    complete_frame[source_at];
            }
        }
    }
}

} // namespace skyroads
