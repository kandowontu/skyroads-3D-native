#include "recovered_game.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

struct TemporaryDirectory {
    std::filesystem::path path;

    explicit TemporaryDirectory(const char* suffix) {
        const auto stamp = std::chrono::high_resolution_clock::now()
            .time_since_epoch().count();
        path = std::filesystem::temp_directory_path() /
            (std::string("skyroads_embedded_") + suffix + "_" +
             std::to_string(stamp));
        std::filesystem::create_directories(path);
    }

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
};

void verify_executable_only(
    const std::filesystem::path& source_executable,
    const char* destination_name,
    const char* suffix) {
    TemporaryDirectory directory(suffix);
    std::filesystem::copy_file(
        source_executable,
        directory.path / destination_name,
        std::filesystem::copy_options::overwrite_existing);
    skyroads::RecoveredGame game(directory.path);
    require(game.screen() == skyroads::NativeScreen::Intro,
        "Executable-only folder did not initialize the embedded game");
    require(game.indexed_pixels().size() == 320u * 200u,
        "Executable-only folder produced an invalid framebuffer");
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            throw std::runtime_error(
                "usage: embedded_data_tests SKYROADS.EXE [SKYXMAS.EXE]");
        }

        {
            TemporaryDirectory empty("missing_exe");
            bool rejected = false;
            try {
                skyroads::RecoveredGame game(empty.path);
            }
            catch (const std::exception&) {
                rejected = true;
            }
            require(rejected,
                "Folder without either original executable was accepted");
        }

        verify_executable_only(argv[1], "SKYROADS.EXE", "base");
        if (argc >= 3) {
            verify_executable_only(argv[2], "SKYXMAS.EXE", "xmas");
        }
        std::cout << "Embedded archives require only an original DOS executable\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
