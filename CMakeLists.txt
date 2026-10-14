# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(geometry_wars_retro_evolved_2 VERSION 0.2.3 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Where tools/codegen.ps1, tools/codegen.sh and tools/package-linux.sh unpack the pinned SDK.
if(WIN32)
    set(GW2_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tools/rexglue/win-amd64")
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
else()
    list(APPEND GW2_SOURCES src/launcher_platform_linux.cpp)
    add_executable(geometry_wars_retro_evolved_2 ${GW2_SOURCES})
endif()

rexglue_setup_target(geometry_wars_retro_evolved_2 GPU_PLUGINS xenos)
target_link_libraries(geometry_wars_retro_evolved_2 PRIVATE trg::launcher)
if(WIN32)
    target_link_libraries(geometry_wars_retro_evolved_2 PRIVATE bcrypt shcore)
else()
    # The SDK stages the runtime beside the program only on Windows and macOS; the RPATH is $ORIGIN.
    add_custom_command(TARGET geometry_wars_retro_evolved_2 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:rex::runtime>"
            "$<TARGET_FILE_DIR:geometry_wars_retro_evolved_2>"
        VERBATIM
    )
endif()

# Every build checks that tools/rexglue holds the runtime and GPU plugin this branch's patches need, and that the
# copies next to the program are those; tools/build.ps1 and tools/build.sh prepare them when they are not.
if(WIN32)
    find_program(GW2_POWERSHELL NAMES pwsh powershell REQUIRED)
    set(GW2_REXGLUE_CHECK "${GW2_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass
        -File "${CMAKE_CURRENT_SOURCE_DIR}/tools/build-patched-rexruntime.ps1" -Check -Config $<CONFIG>)
    set(GW2_REXGLUE_STAGED -Staged)
else()
    find_program(GW2_BASH bash REQUIRED)
    set(GW2_REXGLUE_CHECK "${GW2_BASH}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/build-patched-rexruntime.sh"
        --check "${GW2_SDK_DIR}" --config $<CONFIG>)
    set(GW2_REXGLUE_STAGED --staged)
endif()
get_property(_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT _multi_config AND NOT CMAKE_BUILD_TYPE MATCHES "^(Debug|RelWithDebInfo|Release)$")
    message(FATAL_ERROR "Set CMAKE_BUILD_TYPE to Debug, RelWithDebInfo or Release (the presets, tools/build.ps1 "
        "and tools/build.sh do): the ReXGlue runtime and GPU plugin are prepared and checked per configuration")
endif()
add_custom_target(gw2_rexglue_check COMMAND ${GW2_REXGLUE_CHECK} VERBATIM)
add_dependencies(geometry_wars_retro_evolved_2 gw2_rexglue_check)
add_custom_target(gw2_rexglue_staging ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:rex::runtime>" "$<TARGET_FILE:rex::gpu-xenos>"
        "$<TARGET_FILE_DIR:geometry_wars_retro_evolved_2>"
    COMMAND ${GW2_REXGLUE_CHECK} ${GW2_REXGLUE_STAGED} "$<TARGET_FILE_DIR:geometry_wars_retro_evolved_2>"
    VERBATIM
)
add_dependencies(gw2_rexglue_staging geometry_wars_retro_evolved_2)

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

# Tests that need the ReXGlue runtime, not part of the default build: cmake --build <dir> --target <name>
add_executable(gw2_physical_heap_stress EXCLUDE_FROM_ALL tests/physical_heap_stress.cpp)
target_link_libraries(gw2_physical_heap_stress PRIVATE rex::runtime)
add_executable(gw2_restart_state_tests EXCLUDE_FROM_ALL tests/restart_state_tests.cpp src/launcher_cvars.cpp
    src/launcher_settings.cpp src/launcher_options.cpp)
target_include_directories(gw2_restart_state_tests PRIVATE src)
target_compile_options(gw2_restart_state_tests PRIVATE -Wall -Wextra)
target_compile_definitions(gw2_restart_state_tests PRIVATE GW2_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(gw2_restart_state_tests PRIVATE rex::runtime gw2_trg_test_support)
set(_runtime_tests gw2_physical_heap_stress gw2_restart_state_tests)
# The fake librenderdoc.so goes into renderdoc/, out of reach of the runtime's $ORIGIN RUNPATH.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_library(gw2_fake_renderdoc SHARED EXCLUDE_FROM_ALL tests/fake_renderdoc.cpp)
    set_target_properties(gw2_fake_renderdoc PROPERTIES OUTPUT_NAME renderdoc
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/renderdoc")
    add_executable(gw2_renderdoc_detect_tests EXCLUDE_FROM_ALL tests/renderdoc_detect_tests.cpp)
    target_link_libraries(gw2_renderdoc_detect_tests PRIVATE rex::runtime rex::renderdoc ${CMAKE_DL_LIBS})
    target_compile_options(gw2_renderdoc_detect_tests PRIVATE -Wall -Wextra)
    add_dependencies(gw2_renderdoc_detect_tests gw2_fake_renderdoc)
    list(APPEND _runtime_tests gw2_renderdoc_detect_tests)
endif()
foreach(_runtime_test ${_runtime_tests})
    add_custom_command(TARGET ${_runtime_test} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:rex::runtime>"
            "$<TARGET_FILE_DIR:${_runtime_test}>"
        VERBATIM
    )
    if(NOT WIN32)
        set_target_properties(${_runtime_test} PROPERTIES BUILD_RPATH "$ORIGIN")
    endif()
endforeach()
