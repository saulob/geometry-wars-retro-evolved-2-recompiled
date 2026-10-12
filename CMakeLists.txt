# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(geometry_wars_retro_evolved_2 VERSION 0.2.2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Where tools/codegen.ps1, tools/codegen.sh and tools/package-linux.sh unpack the pinned SDK.
if(WIN32)
    set(GW2_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tools/rexglue/win-amd64")
elseif(APPLE)
    set(GW2_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tools/rexglue/mac-arm64")
    if(NOT EXISTS "${GW2_SDK_DIR}/rexgpu-xenos-patch.txt")
        message(WARNING "The stock ReXGlue GPU plugin draws this game far too dark on Vulkan: "
            "run tools/build-patched-rexgpu-xenos.sh (see docs/rexglue-gpu-patch.md)")
    endif()
else()
    set(GW2_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tools/rexglue/linux-amd64")
endif()
list(APPEND CMAKE_PREFIX_PATH "${GW2_SDK_DIR}")

# Release binaries must not carry the build machine's folders (__FILE__ in log calls).
add_compile_options("$<$<CONFIG:Release>:-fmacro-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=.>")

include(generated/rexglue.cmake)

# TRG Launcher: the third_party/trg-launcher submodule.
set(_trg_dir "${CMAKE_CURRENT_SOURCE_DIR}/third_party/trg-launcher")
if(NOT EXISTS "${_trg_dir}/CMakeLists.txt")
    message(FATAL_ERROR "TRG Launcher not found: run `git submodule update --init third_party/trg-launcher`")
endif()

# The launcher draws inside the runtime's own Dear ImGui frame, so it must use
# the ImGui compiled into rexruntime (one context), never a downloaded copy.
add_library(gw2_imgui INTERFACE)
target_link_libraries(gw2_imgui INTERFACE rex::runtime)
set(TRG_LAUNCHER_IMGUI_TARGET gw2_imgui CACHE STRING "" FORCE)
set(TRG_LAUNCHER_STANDALONE OFF CACHE BOOL "" FORCE)
set(TRG_LAUNCHER_BUILD_DEMO OFF CACHE BOOL "" FORCE)
set(TRG_LAUNCHER_FETCH_DEPS OFF CACHE BOOL "" FORCE)
add_subdirectory("${_trg_dir}" trg-launcher)
# Written for C++17: in C++20 and later u8string() is a different type.
set_target_properties(trg_launcher PROPERTIES CXX_STANDARD 17)
# TRG defines this only for cl.exe; clang++ needs it too for the MSVC CRT.
target_compile_definitions(trg_launcher PRIVATE _CRT_SECURE_NO_WARNINGS)
get_directory_property(GW2_TRG_VERSION DIRECTORY "${_trg_dir}" DEFINITION PROJECT_VERSION)

set(GW2_SOURCES
    src/main.cpp
    src/profile_fix.cpp
    src/live_fix.cpp
    src/achievements_overlay.cpp
    src/cheat_menu.cpp
    src/cheat_hooks.cpp
    src/cheats.cpp
    src/bloom_fix.cpp
    src/live_stats.cpp
    src/local_stats.cpp
    src/game_files.cpp
    src/stfs.cpp
    src/frame_counter.cpp
    src/frame_limiter.cpp
    src/frame_wait.cpp
    src/launcher.cpp
    src/launcher_achievements.cpp
    src/launcher_cvars.cpp
    src/launcher_options.cpp
    src/launcher_platform.cpp
    src/launcher_settings.cpp
    src/user_data.cpp
    src/window_host.cpp
)
if(WIN32)
    # Resource 1 is the first icon in the executable: Explorer, and SDL's window class, use it.
    enable_language(RC)
    set(GW2_APP_RC "${CMAKE_CURRENT_SOURCE_DIR}/packaging/windows/geometry_wars_retro_evolved_2.rc")
    set_source_files_properties("${GW2_APP_RC}" PROPERTIES
        OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icons/app-icon.ico")
    list(APPEND GW2_SOURCES src/launcher_platform_win32.cpp "${GW2_APP_RC}")
    add_executable(geometry_wars_retro_evolved_2 WIN32 ${GW2_SOURCES})
elseif(APPLE)
    list(APPEND GW2_SOURCES src/launcher_platform_mac.cpp)
    add_executable(geometry_wars_retro_evolved_2 ${GW2_SOURCES})
else()
    list(APPEND GW2_SOURCES src/launcher_platform_linux.cpp)
    add_executable(geometry_wars_retro_evolved_2 ${GW2_SOURCES})
endif()

rexglue_setup_target(geometry_wars_retro_evolved_2 GPU_PLUGINS xenos)
target_link_libraries(geometry_wars_retro_evolved_2 PRIVATE trg::launcher)
if(WIN32)
    target_link_libraries(geometry_wars_retro_evolved_2 PRIVATE bcrypt shcore)
elseif(APPLE)
    # The SDK copies the GPU plugin beside the program after each link; relink when the patched plugin is installed.
    set_property(TARGET geometry_wars_retro_evolved_2 APPEND PROPERTY LINK_DEPENDS "$<TARGET_FILE:rex::gpu-xenos>")
else()
    # The SDK stages the runtime beside the program only on Windows and macOS; the RPATH is $ORIGIN.
    add_custom_command(TARGET geometry_wars_retro_evolved_2 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:rex::runtime>"
            "$<TARGET_FILE_DIR:geometry_wars_retro_evolved_2>"
        VERBATIM
    )
endif()
target_compile_definitions(geometry_wars_retro_evolved_2 PRIVATE
    GW2_PORT_VERSION="${PROJECT_VERSION}"
    GW2_TRG_LAUNCHER_VERSION="${GW2_TRG_VERSION}")

# The runtime reads its cvars from <exe dir>/geometry_wars_retro_evolved_2.toml.
add_custom_command(TARGET geometry_wars_retro_evolved_2 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/config/geometry_wars_retro_evolved_2.toml"
        "$<TARGET_FILE_DIR:geometry_wars_retro_evolved_2>"
    VERBATIM
)

# Unit tests need no game files and no ReXGlue runtime. They are not part of the default build:
#   cmake --build <dir> --target gw2_tests
# and the programs land in <dir>/tests. On Windows tools/test.ps1 builds and runs the same set.
add_custom_target(gw2_tests)
function(gw2_add_test name)
    add_executable(${name} EXCLUDE_FROM_ALL tests/${name}.cpp ${ARGN})
    target_include_directories(${name} PRIVATE src)
    target_include_directories(${name} SYSTEM PRIVATE "${GW2_SDK_DIR}/include" "${_trg_dir}/include")
    target_compile_options(${name} PRIVATE -Wall -Wextra)
    target_compile_definitions(${name} PRIVATE _CRT_SECURE_NO_WARNINGS _SILENCE_ALL_CXX20_DEPRECATION_WARNINGS)
    set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    if(NOT WIN32)
        target_link_libraries(${name} PRIVATE Threads::Threads)
    endif()
    add_dependencies(gw2_tests ${name})
endfunction()
if(NOT WIN32)
    find_package(Threads REQUIRED)
endif()

gw2_add_test(live_tests src/local_stats.cpp src/live_stats.cpp)
gw2_add_test(achievements_overlay_rules_tests)
gw2_add_test(launcher_tests src/game_files.cpp src/stfs.cpp src/launcher_achievements.cpp)
gw2_add_test(window_host_tests)
gw2_add_test(frame_counter_tests)
gw2_add_test(frame_limiter_tests src/frame_wait.cpp)
gw2_add_test(launcher_placement_tests)
gw2_add_test(user_data_tests src/user_data.cpp)
gw2_add_test(cheats_tests)
target_compile_definitions(cheats_tests PRIVATE GW2_SRC_DIR="${CMAKE_CURRENT_SOURCE_DIR}/src")
# TRG's settings and platform sources without its ImGui dependency; C++17 like the launcher library.
add_library(gw2_trg_test_support STATIC EXCLUDE_FROM_ALL
    "${_trg_dir}/src/settings.cpp" "${_trg_dir}/src/platform.cpp")
target_include_directories(gw2_trg_test_support PUBLIC "${_trg_dir}/include")
set_target_properties(gw2_trg_test_support PROPERTIES CXX_STANDARD 17)
target_compile_definitions(gw2_trg_test_support PRIVATE _CRT_SECURE_NO_WARNINGS)
gw2_add_test(launcher_settings_tests src/launcher_settings.cpp src/launcher_options.cpp)
target_link_libraries(launcher_settings_tests PRIVATE gw2_trg_test_support)
if(WIN32)
    target_link_libraries(launcher_tests PRIVATE bcrypt)
    target_link_libraries(gw2_trg_test_support PUBLIC shell32 ole32 comdlg32 winmm user32)
else()
    target_link_libraries(gw2_trg_test_support PUBLIC Threads::Threads)
endif()

# Allocator stress test, not part of the default build: cmake --build <dir> --target gw2_physical_heap_stress
add_executable(gw2_physical_heap_stress EXCLUDE_FROM_ALL tests/physical_heap_stress.cpp)
target_link_libraries(gw2_physical_heap_stress PRIVATE rex::runtime)
add_custom_command(TARGET gw2_physical_heap_stress POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:rex::runtime>"
        "$<TARGET_FILE_DIR:gw2_physical_heap_stress>"
    VERBATIM
)
if(APPLE)
    set_target_properties(gw2_physical_heap_stress PROPERTIES BUILD_RPATH "@executable_path")
elseif(NOT WIN32)
    set_target_properties(gw2_physical_heap_stress PROPERTIES BUILD_RPATH "$ORIGIN")
endif()
