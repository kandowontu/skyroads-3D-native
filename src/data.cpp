#include "data.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <sstream>

namespace skyroads {
namespace {

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) {
        throw DataError("Unable to open " + path.string());
    }
    const auto end = stream.tellg();
    if (end < 0) {
        throw DataError("Unable to determine size of " + path.string());
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
    stream.seekg(0);
    if (!bytes.empty() && !stream.read(reinterpret_cast<char*>(bytes.data()), end)) {
        throw DataError("Unable to read " + path.string());
    }
    return bytes;
}

std::uint16_t read_u16(std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset + 2 > bytes.size()) {
        throw DataError("Unexpected end of data while reading a 16-bit value");
    }
    return static_cast<std::uint16_t>(bytes[offset]) |
           static_cast<std::uint16_t>(bytes[offset + 1] << 8U);
}

bool has_signature(std::span<const std::uint8_t> bytes, std::size_t offset, const char* signature) {
    return offset + 4 <= bytes.size() &&
           bytes[offset] == static_cast<std::uint8_t>(signature[0]) &&
           bytes[offset + 1] == static_cast<std::uint8_t>(signature[1]) &&
           bytes[offset + 2] == static_cast<std::uint8_t>(signature[2]) &&
           bytes[offset + 3] == static_cast<std::uint8_t>(signature[3]);
}

class BitReader {
public:
    BitReader(std::span<const std::uint8_t> bytes, std::size_t offset)
        : bytes_(bytes), start_(offset), byte_(offset) {}

    std::uint32_t read(unsigned count) {
        std::uint32_t value = 0;
        for (unsigned index = 0; index < count; ++index) {
            if (byte_ >= bytes_.size()) {
                throw DataError("Unexpected end of compressed stream");
            }
            const auto bit = (bytes_[byte_] >> (7U - bit_)) & 1U;
            value = (value << 1U) | bit;
            if (++bit_ == 8) {
                bit_ = 0;
                ++byte_;
            }
        }
        return value;
    }

    [[nodiscard]] std::size_t consumed() const {
        return byte_ + (bit_ == 0 ? 0 : 1) - start_;
    }

private:
    std::span<const std::uint8_t> bytes_;
    std::size_t start_{};
    std::size_t byte_{};
    unsigned bit_{};
};

std::size_t parse_palette(
    std::span<const std::uint8_t> bytes,
    std::size_t signature_offset,
    ImageArchive& archive) {
    if (!has_signature(bytes, signature_offset, "CMAP") || signature_offset + 5 > bytes.size()) {
        throw DataError("Invalid or truncated CMAP chunk");
    }
    const auto count = static_cast<std::size_t>(bytes[signature_offset + 4]);
    const auto colors_start = signature_offset + 5;
    const auto colors_end = colors_start + count * 3;
    const auto chunk_end = colors_end + count * 2;
    if (chunk_end > bytes.size()) {
        throw DataError("Truncated CMAP palette data");
    }

    ImagePalette palette;
    palette.colors.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        const auto base = colors_start + index * 3;
        const auto expand = [](std::uint8_t channel) {
            return static_cast<std::uint8_t>((static_cast<unsigned>(channel) * 255U) / 63U);
        };
        palette.colors.push_back({expand(bytes[base]), expand(bytes[base + 1]), expand(bytes[base + 2])});
    }
    palette.auxiliary.assign(bytes.begin() + static_cast<std::ptrdiff_t>(colors_end),
                             bytes.begin() + static_cast<std::ptrdiff_t>(chunk_end));
    archive.palettes.push_back(std::move(palette));
    return chunk_end;
}

std::pair<ImageFrame, std::size_t> parse_picture(
    std::span<const std::uint8_t> bytes,
    std::size_t signature_offset,
    std::size_t palette_index) {
    if (!has_signature(bytes, signature_offset, "PICT") || signature_offset + 13 > bytes.size()) {
        throw DataError("Invalid or truncated PICT chunk");
    }
    ImageFrame frame;
    frame.screen_offset = read_u16(bytes, signature_offset + 4);
    const auto stored_height = read_u16(bytes, signature_offset + 6);
    frame.width = read_u16(bytes, signature_offset + 8);
    frame.height = stored_height == 0 ? 1 : stored_height;
    frame.palette_index = palette_index;
    const CompressionWidths widths{
        bytes[signature_offset + 10], bytes[signature_offset + 11], bytes[signature_offset + 12]};
    const auto expected = static_cast<std::size_t>(std::max<std::uint16_t>(frame.width, 1)) *
                          static_cast<std::size_t>(frame.height);
    auto decompressed = decompress_lzs(bytes, signature_offset + 13, expected, widths);
    frame.pixels = std::move(decompressed.bytes);
    return {std::move(frame), signature_offset + 13 + decompressed.consumed};
}

std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

} // namespace

DecompressedStream decompress_lzs(
    std::span<const std::uint8_t> source,
    std::size_t offset,
    std::optional<std::size_t> expected_size,
    CompressionWidths widths) {
    if (offset > source.size() || widths.count > 24 || widths.short_distance > 24 ||
        widths.long_distance > 24) {
        throw DataError("Invalid SkyRoads compression parameters");
    }
    BitReader reader(source, offset);
    std::vector<std::uint8_t> output;
    if (expected_size) {
        output.reserve(*expected_size);
    }

    auto copy_history = [&](std::size_t distance, std::size_t count) {
        if (distance == 0 || distance > output.size()) {
            throw DataError("Invalid SkyRoads LZS history distance");
        }
        for (std::size_t index = 0; index < count; ++index) {
            if (expected_size && output.size() >= *expected_size) {
                break;
            }
            output.push_back(output[output.size() - distance]);
        }
    };

    try {
        while (!expected_size || output.size() < *expected_size) {
            if (reader.read(1) == 0) {
                const auto distance = static_cast<std::size_t>(reader.read(widths.short_distance)) + 2;
                const auto count = static_cast<std::size_t>(reader.read(widths.count)) + 2;
                copy_history(distance, count);
            } else if (reader.read(1) == 0) {
                const auto distance = static_cast<std::size_t>(reader.read(widths.long_distance)) + 2 +
                                      (std::size_t{1} << widths.short_distance);
                const auto count = static_cast<std::size_t>(reader.read(widths.count)) + 2;
                copy_history(distance, count);
            } else {
                output.push_back(static_cast<std::uint8_t>(reader.read(8)));
            }
        }
    } catch (const DataError&) {
        if (expected_size) {
            throw;
        }
    }

    return {std::move(output), reader.consumed()};
}

ImageArchive load_image_archive(const std::filesystem::path& path) {
    const auto owned = read_file(path);
    const std::span<const std::uint8_t> bytes(owned);
    ImageArchive archive;

    if (has_signature(bytes, 0, "ANIM")) {
        archive.declared_animation_frames = read_u16(bytes, 4);
        auto cursor = parse_palette(bytes, 6, archive);
        for (std::size_t frame_index = 0; frame_index < *archive.declared_animation_frames; ++frame_index) {
            const auto part_count = read_u16(bytes, cursor);
            cursor += 2;
            std::vector<std::size_t> parts;
            parts.reserve(part_count);
            for (std::size_t part = 0; part < part_count; ++part) {
                auto [fragment, next] = parse_picture(bytes, cursor, archive.palettes.size() - 1);
                parts.push_back(archive.fragments.size());
                archive.fragments.push_back(std::move(fragment));
                cursor = next;
            }
            archive.frames.push_back(std::move(parts));
        }
        if (cursor != bytes.size()) {
            throw DataError("ANIM archive has trailing or unparsed bytes: " + path.string());
        }
        return archive;
    }

    if (!has_signature(bytes, 0, "CMAP")) {
        throw DataError("Image archive does not begin with CMAP or ANIM: " + path.string());
    }
    std::size_t cursor = 0;
    std::optional<std::size_t> palette_index;
    while (cursor < bytes.size()) {
        if (has_signature(bytes, cursor, "CMAP")) {
            cursor = parse_palette(bytes, cursor, archive);
            palette_index = archive.palettes.size() - 1;
        } else if (has_signature(bytes, cursor, "PICT")) {
            if (!palette_index) {
                throw DataError("PICT appeared before CMAP in " + path.string());
            }
            auto [fragment, next] = parse_picture(bytes, cursor, *palette_index);
            archive.frames.push_back({archive.fragments.size()});
            archive.fragments.push_back(std::move(fragment));
            cursor = next;
        } else {
            std::ostringstream message;
            message << "Unknown image chunk at byte " << cursor << " in " << path.string();
            throw DataError(message.str());
        }
    }
    return archive;
}

RoadArchive load_road_archive(const std::filesystem::path& path) {
    const auto owned = read_file(path);
    const std::span<const std::uint8_t> bytes(owned);
    if (bytes.size() < 4) {
        throw DataError("ROADS.LZS is truncated");
    }
    const auto first_offset = static_cast<std::size_t>(read_u16(bytes, 0));
    if (first_offset == 0 || first_offset % 4 != 0 || first_offset > bytes.size()) {
        throw DataError("ROADS.LZS has an invalid index size");
    }
    const auto count = first_offset / 4;
    RoadArchive archive;
    archive.roads.reserve(count);

    for (std::size_t index = 0; index < count; ++index) {
        const auto entry = index * 4;
        const auto offset = static_cast<std::size_t>(read_u16(bytes, entry));
        const auto expected_size = static_cast<std::size_t>(read_u16(bytes, entry + 2));
        const auto next_offset = index + 1 < count
                                     ? static_cast<std::size_t>(read_u16(bytes, entry + 4))
                                     : bytes.size();
        if (offset < first_offset || offset + 225 > next_offset || next_offset > bytes.size()) {
            throw DataError("ROADS.LZS contains an invalid road range");
        }

        Road road;
        road.gravity = read_u16(bytes, offset);
        road.fuel = read_u16(bytes, offset + 2);
        road.oxygen = read_u16(bytes, offset + 4);
        for (std::size_t color = 0; color < road.palette.size(); ++color) {
            const auto base = offset + 6 + color * 3;
            const auto expand = [](std::uint8_t channel) {
                return static_cast<std::uint8_t>((static_cast<unsigned>(channel) * 255U) / 63U);
            };
            road.palette[color] = {expand(bytes[base]), expand(bytes[base + 1]), expand(bytes[base + 2])};
        }
        const CompressionWidths widths{bytes[offset + 222], bytes[offset + 223], bytes[offset + 224]};
        auto decoded = decompress_lzs(bytes.first(next_offset), offset + 225, expected_size, widths);
        if (decoded.bytes.size() % 14 != 0) {
            throw DataError("Road data does not contain seven 16-bit cells per row");
        }
        for (std::size_t row_offset = 0; row_offset < decoded.bytes.size(); row_offset += 14) {
            std::array<std::uint16_t, 7> row{};
            for (std::size_t column = 0; column < row.size(); ++column) {
                row[column] = read_u16(decoded.bytes, row_offset + column * 2);
            }
            road.rows.push_back(row);
        }
        archive.roads.push_back(std::move(road));
    }
    return archive;
}

std::filesystem::path find_data_file(
    const std::filesystem::path& directory,
    const std::string& filename) {
    const auto exact = directory / filename;
    if (std::filesystem::exists(exact)) {
        return exact;
    }
    const auto wanted = lower_ascii(filename);
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
        if (entry.is_regular_file() && lower_ascii(entry.path().filename().string()) == wanted) {
            return entry.path();
        }
    }
    throw DataError("Missing required SkyRoads data file " + filename + " under " + directory.string());
}

Assets Assets::load(const std::filesystem::path& source_root) {
    Assets assets;
    assets.source_root = std::filesystem::absolute(source_root);
    assets.intro = load_image_archive(find_data_file(assets.source_root, "intro.lzs"));
    assets.main_menu = load_image_archive(find_data_file(assets.source_root, "mainmenu.lzs"));
    assets.dashboard = load_image_archive(find_data_file(assets.source_root, "dashbrd.lzs"));
    assets.cars = load_image_archive(find_data_file(assets.source_root, "cars.lzs"));
    for (std::size_t index = 0; index < assets.worlds.size(); ++index) {
        assets.worlds[index] = load_image_archive(
            find_data_file(assets.source_root, "world" + std::to_string(index) + ".lzs"));
    }
    assets.roads = load_road_archive(find_data_file(assets.source_root, "roads.lzs"));
    return assets;
}

} // namespace skyroads
