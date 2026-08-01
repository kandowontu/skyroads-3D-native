cmake_minimum_required(VERSION 3.20)
project(skyroads_recovered LANGUAGES C)

option(SKYROADS_RECOVERED_BUILD_TESTS "Build reconstructed-core tests" ON)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(skyroads_recovered STATIC
    allocation.c
    animation_archive.c
    bios_font.c
    car_sprites.c
    config.c
    dashboard.c
    display_table.c
    embedded_hud.c
    gameplay.c
    graphics_archive.c
    hardware.c
    input.c
    intro.c
    keyboard.c
    level_flow.c
    lzs.c
    menu_flow.c
    menus.c
    motion.c
    music.c
    palette.c
    physics.c
    picture_blitter.c
    render_params.c
    renderer_tables.c
    renderer_vga.c
    road.c
    road_archive.c
    sound_effects.c
    startup.c
    trek_archive.c
)
target_include_directories(skyroads_recovered PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

if(MSVC)
    target_compile_options(skyroads_recovered PRIVATE /W4)
else()
    target_compile_options(skyroads_recovered PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(SKYROADS_RECOVERED_BUILD_TESTS)
enable_testing()
add_executable(recovered_lzs_test test_lzs.c)
target_link_libraries(recovered_lzs_test PRIVATE skyroads_recovered)
add_test(NAME recovered_lzs COMMAND recovered_lzs_test
    "${CMAKE_CURRENT_SOURCE_DIR}/../../intro.lzs"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../roads.lzs"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../trekdat.lzs")

add_executable(recovered_input_test test_input.c)
target_link_libraries(recovered_input_test PRIVATE skyroads_recovered)
add_test(NAME recovered_input COMMAND recovered_input_test
    "${CMAKE_CURRENT_SOURCE_DIR}/../../demo.rec")

add_executable(recovered_road_test test_road.c)
target_link_libraries(recovered_road_test PRIVATE skyroads_recovered)
add_test(NAME recovered_road COMMAND recovered_road_test)

add_executable(recovered_config_test test_config.c)
target_link_libraries(recovered_config_test PRIVATE skyroads_recovered)
add_test(NAME recovered_config COMMAND recovered_config_test)

add_executable(recovered_allocation_test test_allocation.c)
target_link_libraries(recovered_allocation_test PRIVATE skyroads_recovered)
add_test(NAME recovered_allocation COMMAND recovered_allocation_test)

add_executable(recovered_physics_test test_physics.c)
target_link_libraries(recovered_physics_test PRIVATE skyroads_recovered)
add_test(NAME recovered_physics COMMAND recovered_physics_test)

add_executable(recovered_motion_test test_motion.c)
target_link_libraries(recovered_motion_test PRIVATE skyroads_recovered)
add_test(NAME recovered_motion COMMAND recovered_motion_test)

add_executable(recovered_gameplay_test test_gameplay.c)
target_link_libraries(recovered_gameplay_test PRIVATE skyroads_recovered)
add_test(NAME recovered_gameplay COMMAND recovered_gameplay_test)

add_executable(recovered_road_archive_test test_road_archive.c)
target_link_libraries(recovered_road_archive_test PRIVATE skyroads_recovered)
add_test(NAME recovered_road_archive COMMAND recovered_road_archive_test
    "${CMAKE_CURRENT_SOURCE_DIR}/../../roads.lzs")

add_executable(recovered_demo_trace_test test_demo_trace.c)
target_link_libraries(recovered_demo_trace_test PRIVATE skyroads_recovered)
add_test(NAME recovered_demo_trace COMMAND recovered_demo_trace_test
    "${CMAKE_CURRENT_SOURCE_DIR}/../../roads.lzs"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../demo.rec")

add_executable(recovered_render_params_test test_render_params.c)
target_link_libraries(recovered_render_params_test PRIVATE skyroads_recovered)
add_test(NAME recovered_render_params COMMAND recovered_render_params_test)

add_executable(recovered_trek_archive_test test_trek_archive.c)
target_link_libraries(recovered_trek_archive_test PRIVATE skyroads_recovered)
add_test(NAME recovered_trek_archive COMMAND recovered_trek_archive_test
    "${CMAKE_CURRENT_SOURCE_DIR}/../../trekdat.lzs")

add_executable(recovered_renderer_vga_test test_renderer_vga.c)
target_link_libraries(recovered_renderer_vga_test PRIVATE skyroads_recovered)
add_test(NAME recovered_renderer_vga COMMAND recovered_renderer_vga_test
    "${CMAKE_CURRENT_SOURCE_DIR}/../../skyroads.exe"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../trekdat.lzs"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../roads.lzs"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../cars.lzs")

add_executable(recovered_graphics_archive_test test_graphics_archive.c)
target_link_libraries(recovered_graphics_archive_test PRIVATE skyroads_recovered)
add_test(NAME recovered_graphics_archive COMMAND recovered_graphics_archive_test
    "${CMAKE_CURRENT_SOURCE_DIR}/../../cars.lzs"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../dashbrd.lzs"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../gomenu.lzs"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../helpmenu.lzs"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../intro.lzs"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../mainmenu.lzs"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../setmenu.lzs"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../world0.lzs"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../world1.lzs"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../world2.lzs"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../world3.lzs"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../world4.lzs"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../world5.lzs"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../world6.lzs"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../world7.lzs"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../world8.lzs"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../world9.lzs")

add_executable(recovered_animation_archive_test test_animation_archive.c)
target_link_libraries(recovered_animation_archive_test PRIVATE skyroads_recovered)
add_test(NAME recovered_animation_archive COMMAND recovered_animation_archive_test
    "${CMAKE_CURRENT_SOURCE_DIR}/../../anim.lzs")

add_executable(recovered_display_table_test test_display_table.c)
target_link_libraries(recovered_display_table_test PRIVATE skyroads_recovered)
add_test(NAME recovered_display_table COMMAND recovered_display_table_test
    "${CMAKE_CURRENT_SOURCE_DIR}/../../oxy_disp.dat"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../ful_disp.dat"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../speed.dat")

add_executable(recovered_dashboard_test test_dashboard.c)
target_link_libraries(recovered_dashboard_test PRIVATE skyroads_recovered)
add_test(NAME recovered_dashboard COMMAND recovered_dashboard_test
    "${CMAKE_CURRENT_SOURCE_DIR}/../../skyroads.exe"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../dashbrd.lzs"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../world0.lzs"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../roads.lzs"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../oxy_disp.dat"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../ful_disp.dat"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../speed.dat")

add_executable(recovered_level_frame_test test_level_frame.c)
target_link_libraries(recovered_level_frame_test PRIVATE skyroads_recovered)
add_test(NAME recovered_level_frame COMMAND recovered_level_frame_test
    "${CMAKE_CURRENT_SOURCE_DIR}/../../skyroads.exe"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../dashbrd.lzs"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../world0.lzs"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../roads.lzs"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../trekdat.lzs"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../cars.lzs"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../oxy_disp.dat"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../ful_disp.dat"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../speed.dat")

add_executable(recovered_sound_effects_test test_sound_effects.c)
target_link_libraries(recovered_sound_effects_test PRIVATE skyroads_recovered)
add_test(NAME recovered_sound_effects COMMAND recovered_sound_effects_test
    "${CMAKE_CURRENT_SOURCE_DIR}/../../skyroads.exe"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../sfx.snd")

add_executable(recovered_menus_test test_menus.c)
target_link_libraries(recovered_menus_test PRIVATE skyroads_recovered)
add_test(NAME recovered_menus COMMAND recovered_menus_test)

add_executable(recovered_menu_flow_test test_menu_flow.c)
target_link_libraries(recovered_menu_flow_test PRIVATE skyroads_recovered)
add_test(NAME recovered_menu_flow COMMAND recovered_menu_flow_test)

add_executable(recovered_palette_test test_palette.c)
target_link_libraries(recovered_palette_test PRIVATE skyroads_recovered)
add_test(NAME recovered_palette COMMAND recovered_palette_test)

add_executable(recovered_keyboard_test test_keyboard.c)
target_link_libraries(recovered_keyboard_test PRIVATE skyroads_recovered)
add_test(NAME recovered_keyboard COMMAND recovered_keyboard_test)

add_executable(recovered_dos_oracle_compare test_dos_oracle.c)
target_link_libraries(recovered_dos_oracle_compare PRIVATE skyroads_recovered)

add_executable(recovered_dos_opl_oracle_compare test_dos_opl_oracle.c)
target_link_libraries(recovered_dos_opl_oracle_compare PRIVATE skyroads_recovered)

add_executable(recovered_dos_input_oracle_compare test_dos_input_oracle.c)
target_link_libraries(recovered_dos_input_oracle_compare PRIVATE skyroads_recovered)

set(SKYROADS_DOS_ORACLE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../generated/oracle-run"
    CACHE PATH "Directory containing ORASTATE.BIN and ORASFX.BIN")
if(EXISTS "${SKYROADS_DOS_ORACLE_DIR}/ORASTATE.BIN" AND
   EXISTS "${SKYROADS_DOS_ORACLE_DIR}/ORASFX.BIN")
    add_test(NAME recovered_dos_oracle
        COMMAND recovered_dos_oracle_compare
            "${CMAKE_CURRENT_SOURCE_DIR}/../../roads.lzs"
            "${CMAKE_CURRENT_SOURCE_DIR}/../../demo.rec"
            "${SKYROADS_DOS_ORACLE_DIR}/ORASTATE.BIN"
            "${SKYROADS_DOS_ORACLE_DIR}/ORASFX.BIN")
endif()
if(EXISTS "${SKYROADS_DOS_ORACLE_DIR}/ORAOPL.DRO")
    add_test(NAME recovered_dos_opl_oracle
        COMMAND recovered_dos_opl_oracle_compare
            "${CMAKE_CURRENT_SOURCE_DIR}/../../muzax.lzs"
            "${SKYROADS_DOS_ORACLE_DIR}/ORAOPL.DRO")
endif()
if(EXISTS "${SKYROADS_DOS_ORACLE_DIR}/ORAKEY.BIN" AND
   EXISTS "${SKYROADS_DOS_ORACLE_DIR}/ORAJOY.BIN" AND
   EXISTS "${SKYROADS_DOS_ORACLE_DIR}/ORAMOUSE.BIN")
    add_test(NAME recovered_dos_input_oracle
        COMMAND recovered_dos_input_oracle_compare
            "${SKYROADS_DOS_ORACLE_DIR}/ORAKEY.BIN"
            "${SKYROADS_DOS_ORACLE_DIR}/ORAJOY.BIN"
            "${SKYROADS_DOS_ORACLE_DIR}/ORAMOUSE.BIN")
endif()

add_executable(recovered_music_test test_music.c)
target_link_libraries(recovered_music_test PRIVATE skyroads_recovered)
add_test(NAME recovered_music COMMAND recovered_music_test
    "${CMAKE_CURRENT_SOURCE_DIR}/../../muzax.lzs")

add_executable(recovered_intro_test test_intro.c)
target_link_libraries(recovered_intro_test PRIVATE skyroads_recovered)
add_test(NAME recovered_intro COMMAND recovered_intro_test)

add_executable(recovered_level_flow_test test_level_flow.c)
target_link_libraries(recovered_level_flow_test PRIVATE skyroads_recovered)
add_test(NAME recovered_level_flow COMMAND recovered_level_flow_test)
endif()
