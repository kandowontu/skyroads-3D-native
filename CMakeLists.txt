cmake_minimum_required(VERSION 3.23)

project(skyroads_native VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(SKYROADS_DATA_DIR
    "${CMAKE_SOURCE_DIR}"
    CACHE PATH "Path to the original SkyRoads data files used at build time")
set(SKYROADS_XMAS_DATA_DIR
    "${CMAKE_SOURCE_DIR}/../skyxmas"
    CACHE PATH "Path to the original SkyRoads Xmas data files used at build time")

set(SKYROADS_BASE_ASSET_NAMES
    anim.lzs cars.lzs dashbrd.lzs demo.rec ful_disp.dat gomenu.lzs
    helpmenu.lzs intro.lzs intro.snd mainmenu.lzs muzax.lzs oxy_disp.dat
    roads.lzs setmenu.lzs sfx.snd speed.dat trekdat.lzs
    world0.lzs world1.lzs world2.lzs world3.lzs world4.lzs
    world5.lzs world6.lzs world7.lzs world8.lzs world9.lzs)
set(SKYROADS_XMAS_ASSET_NAMES
    gomenu.lzs roads.lzs
    world0.lzs world1.lzs world2.lzs world3.lzs world4.lzs
    world5.lzs world6.lzs world7.lzs world8.lzs world9.lzs)

function(resolve_original_assets root output)
    if(NOT IS_DIRECTORY "${root}")
        message(FATAL_ERROR "Original game data directory does not exist: ${root}")
    endif()
    file(GLOB candidates LIST_DIRECTORIES FALSE "${root}/*")
    set(resolved)
    foreach(name IN ITEMS ${ARGN})
        string(TOLOWER "${name}" wanted)
        set(found "")
        foreach(candidate IN LISTS candidates)
            get_filename_component(candidate_name "${candidate}" NAME)
            string(TOLOWER "${candidate_name}" candidate_lower)
            if(candidate_lower STREQUAL wanted)
                set(found "${candidate}")
                break()
            endif()
        endforeach()
        if(NOT found)
            message(FATAL_ERROR
                "Missing original game build input ${name} in ${root}")
        endif()
        list(APPEND resolved "${found}")
    endforeach()
    set(${output} "${resolved}" PARENT_SCOPE)
endfunction()

resolve_original_assets(
    "${SKYROADS_DATA_DIR}" SKYROADS_BASE_ASSET_INPUTS
    ${SKYROADS_BASE_ASSET_NAMES})

set(SKYROADS_EMBED_ARGS
    --base-dir "${SKYROADS_DATA_DIR}"
    --output "${CMAKE_BINARY_DIR}/generated/embedded_game_data.cpp")
set(SKYROADS_XMAS_ASSET_INPUTS)
if(IS_DIRECTORY "${SKYROADS_XMAS_DATA_DIR}")
    resolve_original_assets(
        "${SKYROADS_XMAS_DATA_DIR}" SKYROADS_XMAS_ASSET_INPUTS
        ${SKYROADS_XMAS_ASSET_NAMES})
    list(APPEND SKYROADS_EMBED_ARGS --xmas-dir "${SKYROADS_XMAS_DATA_DIR}")
endif()

set(SKYROADS_EMBEDDED_DATA_CPP
    "${CMAKE_BINARY_DIR}/generated/embedded_game_data.cpp")
add_custom_command(
    OUTPUT "${SKYROADS_EMBEDDED_DATA_CPP}"
    COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tools/embed_game_data.py" ${SKYROADS_EMBED_ARGS}
    DEPENDS
        "${CMAKE_SOURCE_DIR}/tools/embed_game_data.py"
        ${SKYROADS_BASE_ASSET_INPUTS}
        ${SKYROADS_XMAS_ASSET_INPUTS}
    COMMENT "Embedding original SkyRoads archives into the native binaries"
    VERBATIM)

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
    "${SKYROADS_EMBEDDED_DATA_CPP}"
    src/custom_levels.cpp
    src/editor_3d_view.cpp
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
    skyroads_port_core skyroads_opl_synth user32 gdi32 winmm)

add_executable(skyroads_snapshot src/snapshot_main.cpp)
target_link_libraries(skyroads_snapshot PRIVATE skyroads_port_core)

enable_testing()
add_executable(skyroads_tests tests/data_tests.cpp)
target_link_libraries(skyroads_tests PRIVATE skyroads_port_core)
add_test(NAME skyroads_recovered_integration COMMAND skyroads_tests
    "${CMAKE_SOURCE_DIR}")

add_executable(skyroads_opl_tests tests/opl_synth_tests.cpp)
target_link_libraries(skyroads_opl_tests PRIVATE skyroads_opl_synth)
add_test(NAME skyroads_native_opl COMMAND skyroads_opl_tests "${CMAKE_SOURCE_DIR}")

add_executable(skyroads_expanded_level_menu_tests tests/expanded_level_menu_tests.cpp)
target_link_libraries(skyroads_expanded_level_menu_tests PRIVATE skyroads_port_core)
add_test(NAME skyroads_expanded_level_menu COMMAND skyroads_expanded_level_menu_tests)

add_executable(skyroads_custom_levels_tests tests/custom_levels_tests.cpp)
target_link_libraries(skyroads_custom_levels_tests PRIVATE skyroads_port_core)
add_test(NAME skyroads_custom_levels COMMAND skyroads_custom_levels_tests)

add_executable(skyroads_editor_3d_view_tests tests/editor_3d_view_tests.cpp)
target_link_libraries(skyroads_editor_3d_view_tests PRIVATE skyroads_port_core)
add_test(NAME skyroads_editor_3d_view COMMAND skyroads_editor_3d_view_tests)

add_executable(skyroads_hd_renderer_tests tests/hd_renderer_tests.cpp)
target_link_libraries(skyroads_hd_renderer_tests PRIVATE skyroads_port_core)
add_test(NAME skyroads_hd_renderer COMMAND skyroads_hd_renderer_tests)

add_executable(skyroads_embedded_data_tests tests/embedded_data_tests.cpp)
target_link_libraries(skyroads_embedded_data_tests PRIVATE skyroads_port_core)
set(SKYROADS_BASE_EXE "${SKYROADS_DATA_DIR}/skyroads.exe")
set(SKYROADS_XMAS_EXE "${SKYROADS_XMAS_DATA_DIR}/SKYXMAS.EXE")
if(EXISTS "${SKYROADS_XMAS_EXE}")
    add_test(NAME skyroads_embedded_data COMMAND skyroads_embedded_data_tests
        "${SKYROADS_BASE_EXE}" "${SKYROADS_XMAS_EXE}")
else()
    add_test(NAME skyroads_embedded_data COMMAND skyroads_embedded_data_tests
        "${SKYROADS_BASE_EXE}")
endif()

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
