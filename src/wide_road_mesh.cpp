#include "wide_road_mesh.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace skyroads {
namespace {
unsigned word(const SrTrekRecord& record, std::size_t at) {
    return record.bytes[at] | (record.bytes[at + 1] << 8);
}
double boundary_scale(const SrTrekRecord& record, unsigned row) {
    // The near cap carries the true projected height in its header even when
    // its pixels were cut out by the cockpit. Use the tall cap once the ground
    // header reaches the archive's y=158 sentinel.
    for (unsigned role : {0u, 10u}) {
        auto shape = word(record, row * 48u + 24u + role);
        for (unsigned ordinal = 0; ordinal < 2; ++ordinal) {
            shape += 3;
            while (record.bytes[shape] != 255u) shape += 3;
            ++shape;
        }
        const auto y = (10240u + word(record, shape + 1)) / 320u;
        if (y < 158u) return std::max(0.1,
            (static_cast<double>(y) - 32.0) * 46.0 /
                (role == 0u ? 70.0 : 30.0));
    }
    return -1.0;
}
std::array<double, 12> depths(const SrTrekRecord& record) {
    std::array<double, 12> result{};
    for (unsigned row = 0; row < result.size(); ++row) {
        const auto scale = row <= 10 ? boundary_scale(record, row) : -1.0;
        result[row] = scale > 0 ? 1.0 / scale :
            2.0 * result[row - 1] - result[row - 2];
    }
    return result;
}
}

void build_wide_road_mesh(WideRoadScene& scene, const SrTrekArchive& trek,
    const std::uint16_t* cells, std::size_t row_count, std::uint32_t distance) {
    scene.faces.clear();
    const unsigned phase = (distance >> 13) & 7u;
    const double blend = (distance & 8191u) / 8192.0;
    const auto current = depths(trek.records[phase]);
    const auto next = depths(trek.records[(phase + 1u) & 7u]);
    std::array<double, 11> boundary{};
    for (unsigned r = 0; r < boundary.size(); ++r) {
        const double other = next[r + (phase == 7u ? 1u : 0u)];
        boundary[r] = current[r] + (other - current[r]) * blend;
    }
    const auto cell_at = [&](int row, int col) -> unsigned {
        return row < 0 || row >= static_cast<int>(row_count) || col < 0 || col > 6
            ? 0u : cells[row * 7 + col];
    };
    const auto solid_height = [](unsigned cell) {
        unsigned type = (cell >> 8) & 15u;
        return type >= 4 && type <= 5 ? 40.0 :
            type >= 2 && type <= 3 ? 20.0 : 0.0;
    };
    for (unsigned depth = 1; depth < boundary.size(); ++depth) {
        const int row = static_cast<int>(distance >> 16) + 7 - depth;
        const double far_z = boundary[depth - 1], near_z = boundary[depth];
        if (far_z <= 0.0005) continue;
        for (int col = 0; col < 7; ++col) {
            const auto cell = cell_at(row, col);
            const auto type = (cell >> 8) & 15u;
            if (!cell || type > 5) continue;
            const double left = col - 3.5, right = left + 1;
            const auto vertex = [](double x, double height, double z) {
                return RoadVertex{x, (70.0 - height) / 46.0, z};
            };
            const auto face = [&](std::uint8_t color,
                                  std::initializer_list<RoadVertex> points) {
                scene.faces.push_back({std::vector<RoadVertex>(points), color, 0});
            };
            const auto top = [&](double h, std::uint8_t color) {
                face(color, {vertex(left,h,far_z),vertex(right,h,far_z),
                    vertex(right,h,near_z),vertex(left,h,near_z)});
            };
            const auto side = [&](double x, double low, double high,
                                  std::uint8_t color) {
                face(color, {vertex(x,low,far_z),vertex(x,high,far_z),
                    vertex(x,high,near_z),vertex(x,low,near_z)});
            };
            const auto cap = [&](double z, double low, double high,
                                 std::uint8_t color) {
                face(color, {vertex(left,low,z),vertex(right,low,z),
                    vertex(right,high,z),vertex(left,high,z)});
            };
            const unsigned material = cell & 15u;
            if (material) {
                top(0, static_cast<std::uint8_t>(material));
                top(-7, static_cast<std::uint8_t>(material + 15));
                if (!(cell_at(row,col-1)&15u)) side(left,-7,0,material+45);
                if (!(cell_at(row,col+1)&15u)) side(right,-7,0,material+30);
                if (!(cell_at(row-1,col)&15u)) cap(near_z,-7,0,material+15);
                if (!(cell_at(row+1,col)&15u)) cap(far_z,-7,0,material+15);
            }
            if (!type) continue;
            const auto h = solid_height(cell);
            auto color = static_cast<std::uint8_t>((cell >> 4) & 15u);
            if (!color) color = 61;
            const bool tunnel = type == 1 || type == 3 || type == 5;
            if (h > 0) {
                top(h,color);
                if (solid_height(cell_at(row,col-1)) < h)
                    side(left,0,h,64);
                if (solid_height(cell_at(row,col+1)) < h)
                    side(right,0,h,63);
                if (!tunnel) {
                    if (solid_height(cell_at(row-1,col)) < h) cap(near_z,0,h,62);
                    if (solid_height(cell_at(row+1,col)) < h) cap(far_z,0,h,62);
                    continue;
                }
            }
            // Facets around the arch form an open shell with an inner surface.
            // A solid tunnel cell has the same opening with a flat block roof.
            constexpr std::array<double,7> outer_x{-23,-20,-12,0,12,20,23};
            constexpr std::array<double,7> outer_h{0,10,17,20,17,10,0};
            constexpr std::array<double,7> inner_x{-18,-15,-9,0,9,15,18};
            constexpr std::array<double,7> inner_h{0,8,13,16,13,8,0};
            const double center = (left + right) * .5;
            for (unsigned part=0; part<6; ++part) {
                const double ox0 = center + outer_x[part]/46;
                const double ox1 = center + outer_x[part+1]/46;
                const double ix0 = center + inner_x[part]/46;
                const double ix1 = center + inner_x[part+1]/46;
                const double oh0 = type == 1 ? outer_h[part] : h;
                const double oh1 = type == 1 ? outer_h[part+1] : h;
                const auto shade = static_cast<std::uint8_t>(
                    std::array<unsigned,6>{71,70,69,68,69,70}[part]);
                if (type == 1) face(shade, {vertex(ox0,oh0,far_z),
                    vertex(ox1,oh1,far_z),vertex(ox1,oh1,near_z),vertex(ox0,oh0,near_z)});
                face(shade, {vertex(ix0,inner_h[part],far_z),
                    vertex(ix1,inner_h[part+1],far_z),
                    vertex(ix1,inner_h[part+1],near_z),vertex(ix0,inner_h[part],near_z)});
                for (double z : {near_z,far_z}) {
                    const int neighbor = z == near_z ? row-1 : row+1;
                    const unsigned nt = (cell_at(neighbor,col)>>8)&15u;
                    if (nt==type) continue;
                    face(type==1?67:65,{vertex(ox0,oh0,z),vertex(ox1,oh1,z),
                        vertex(ix1,inner_h[part+1],z),vertex(ix0,inner_h[part],z)});
                }
            }
            if (type != 1) {
                for (double z : {near_z,far_z}) {
                    const int neighbor = z == near_z ? row-1 : row+1;
                    if (((cell_at(neighbor,col)>>8)&15u)==type) continue;
                    face(65,{vertex(left,0,z),vertex(left,h,z),
                        vertex(center-18.0/46,h,z),vertex(center-18.0/46,0,z)});
                    face(65,{vertex(right,0,z),vertex(right,h,z),
                        vertex(center+18.0/46,h,z),vertex(center+18.0/46,0,z)});
                }
            }
        }
    }
}
}
