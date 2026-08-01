cmake_minimum_required(VERSION 3.23)

project(skyroads_native VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
set(OPAL_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(opal
    GIT_REPOSITORY https://github.com/RealBitdancer/opal.git
    GIT_TAG 49f2007f1cc32c81561cb8a485734e1b58a843f7
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(opal)

set(SKYROADS_RECOVERED_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(reverse/reconstructed)

add_library(skyroads_port_core
    src/custom_levels.cpp
    src/expanded_level_menu.cpp
    src/hd_renderer.cpp
    src/recovered_game.cpp)
target_include_directories(skyroads_port_core PUBLIC src)
target_link_libraries(skyroads_port_core PUBLIC skyroads_recovered)
target_compile_definitions(skyroads_port_core PUBLIC UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)

if(MSVC)
    target_compile_options(skyroads_port_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(skyroads_port_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(skyroads_opl_synth src/opl_synth.cpp)
target_include_directories(skyroads_opl_synth PUBLIC src)
target_link_libraries(skyroads_opl_synth PUBLIC opal skyroads_port_core)

add_executable(skyroads_native WIN32
    src/win32_main.cpp
    src/win32_opl_audio.cpp)
target_link_libraries(skyroads_native PRIVATE
    skyroads_port_core skyroads_opl_synth user32 gdi32 shell32 winmm)

add_executable(skyroads_snapshot src/snapshot_main.cpp)
target_link_libraries(skyroads_snapshot PRIVATE skyroads_port_core)

enable_testing()
set(SKYROADS_XMAS_DATA_DIR
    "${CMAKE_SOURCE_DIR}/../skyxmas"
    CACHE PATH "Path to the original SkyRoads Xmas data files")
add_executable(skyroads_tests tests/data_tests.cpp)
target_link_libraries(skyroads_tests PRIVATE skyroads_port_core)
if(EXISTS "${SKYROADS_XMAS_DATA_DIR}/ROADS.LZS")
    add_test(NAME skyroads_recovered_integration COMMAND skyroads_tests
        "${CMAKE_SOURCE_DIR}" "${SKYROADS_XMAS_DATA_DIR}")
else()
    add_test(NAME skyroads_recovered_integration COMMAND skyroads_tests
        "${CMAKE_SOURCE_DIR}")
endif()

add_executable(skyroads_opl_tests tests/opl_synth_tests.cpp)
target_link_libraries(skyroads_opl_tests PRIVATE skyroads_opl_synth)
add_test(NAME skyroads_native_opl COMMAND skyroads_opl_tests "${CMAKE_SOURCE_DIR}")

add_executable(skyroads_expanded_level_menu_tests tests/expanded_level_menu_tests.cpp)
target_link_libraries(skyroads_expanded_level_menu_tests PRIVATE skyroads_port_core)
add_test(NAME skyroads_expanded_level_menu COMMAND skyroads_expanded_level_menu_tests)

add_executable(skyroads_custom_levels_tests tests/custom_levels_tests.cpp)
target_link_libraries(skyroads_custom_levels_tests PRIVATE skyroads_port_core)
add_test(NAME skyroads_custom_levels COMMAND skyroads_custom_levels_tests)

add_executable(skyroads_hd_renderer_tests tests/hd_renderer_tests.cpp)
target_link_libraries(skyroads_hd_renderer_tests PRIVATE skyroads_port_core)
add_test(NAME skyroads_hd_renderer COMMAND skyroads_hd_renderer_tests)

set(SKYROADS_DOS_FRAME_ORACLE
    "${CMAKE_SOURCE_DIR}/reverse/generated/oracle-run/ORAFRAME.BIN"
    CACHE FILEPATH "Path to the captured DOS VGA framebuffer oracle")
option(SKYROADS_ENABLE_DOS_FRAME_ORACLE
    "Register the complete executable-authoritative VGA comparison with CTest" OFF)
add_executable(skyroads_frame_oracle_tests tests/dos_frame_oracle_tests.cpp)
target_link_libraries(skyroads_frame_oracle_tests PRIVATE skyroads_port_core)
if(SKYROADS_ENABLE_DOS_FRAME_ORACLE AND EXISTS "${SKYROADS_DOS_FRAME_ORACLE}")
    add_test(NAME skyroads_dos_vga_frames
        COMMAND skyroads_frame_oracle_tests
            "${CMAKE_SOURCE_DIR}" "${SKYROADS_DOS_FRAME_ORACLE}")
endif()

set(SKYROADS_DOS_PALETTE_ORACLE
    "${CMAKE_SOURCE_DIR}/reverse/generated/oracle-run/ORAPAL.BIN"
    CACHE FILEPATH "Path to the captured DOS VGA DAC oracle")
option(SKYROADS_ENABLE_DOS_PALETTE_ORACLE
    "Register the executable-authoritative VGA palette comparison with CTest" OFF)
add_executable(skyroads_palette_oracle_tests tests/dos_palette_oracle_tests.cpp)
target_link_libraries(skyroads_palette_oracle_tests PRIVATE skyroads_port_core)
if(SKYROADS_ENABLE_DOS_PALETTE_ORACLE AND EXISTS "${SKYROADS_DOS_PALETTE_ORACLE}")
    add_test(NAME skyroads_dos_vga_palettes
        COMMAND skyroads_palette_oracle_tests
            "${CMAKE_SOURCE_DIR}" "${SKYROADS_DOS_PALETTE_ORACLE}")
endif()
