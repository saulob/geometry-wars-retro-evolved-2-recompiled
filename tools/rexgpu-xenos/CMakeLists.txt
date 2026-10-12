# Builds only ReXGlue's Xenos GPU plugin (librexgpu-xenos.dylib) from a patched ReXGlue source tree, against the
# prebuilt runtime of an extracted macOS SDK, with the SDK's own global compile flags. Run by
# tools/build-patched-rexgpu-xenos.sh; see patches/rexglue-sdk/0002-spirv-texture-exp-adjust.patch.
cmake_minimum_required(VERSION 3.27)
project(rexgpu_xenos_patched LANGUAGES C CXX)

if(NOT APPLE)
    message(FATAL_ERROR "This plugin build is for the macOS SDK only")
endif()
set(REXGLUE_SRC "" CACHE PATH "Patched ReXGlue source tree")
set(REXGLUE_SDK "" CACHE PATH "Extracted ReXGlue SDK whose runtime the plugin links against")
if(NOT EXISTS "${REXGLUE_SRC}/src/graphics/CMakeLists.txt" OR NOT EXISTS "${REXGLUE_SDK}/lib/cmake/rexglue")
    message(FATAL_ERROR "Set REXGLUE_SRC to the ReXGlue source tree and REXGLUE_SDK to the extracted SDK")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

list(APPEND CMAKE_PREFIX_PATH "${REXGLUE_SDK}")
find_package(rexglue 0.10.0 EXACT REQUIRED CONFIG)

# The SDK's global flags (its top-level CMakeLists.txt): the plugin shares structures with the prebuilt runtime.
add_compile_options(
    -fno-strict-aliasing
    -ffp-model=strict
    $<$<CONFIG:RELEASE>:-O3>
    $<$<CONFIG:RELEASE>:-DNDEBUG>
    -fno-char8_t
)
add_compile_definitions(REX_PLATFORM_MAC=1 REXGLUE_BUILD_CONFIG="$<CONFIG>")

# The common, SPIR-V and Vulkan sources of src/graphics/CMakeLists.txt.
set(G "${REXGLUE_SRC}/src/graphics")
add_library(rexgpu-xenos SHARED
    ${G}/plugin_main.cpp
    ${G}/flags.cpp
    ${G}/register_file.cpp
    ${G}/registers.cpp
    ${G}/pipeline/shader/shader.cpp
    ${G}/format/ucode.cpp
    ${G}/sampler_info.cpp
    ${G}/graphics_system.cpp
    ${G}/command_processor.cpp
    ${G}/util/draw_extent_estimator.cpp
    ${G}/util/draw.cpp
    ${G}/packet_disassembler.cpp
    ${G}/primitive_processor.cpp
    ${G}/pipeline/render_target/cache.cpp
    ${G}/shared_memory.cpp
    ${G}/pipeline/texture/cache.cpp
    ${G}/pipeline/shader/interpreter.cpp
    ${G}/pipeline/shader/translator.cpp
    ${G}/pipeline/shader/translator_disasm.cpp
    ${G}/pipeline/shader/spirv_builder.cpp
    ${G}/pipeline/shader/spirv.cpp
    ${G}/pipeline/shader/spirv_translator.cpp
    ${G}/pipeline/shader/spirv_translator_alu.cpp
    ${G}/pipeline/shader/spirv_translator_fetch.cpp
    ${G}/pipeline/shader/spirv_translator_memexport.cpp
    ${G}/pipeline/shader/spirv_translator_rb.cpp
    ${G}/vulkan/graphics_system.cpp
    ${G}/vulkan/deferred_command_buffer.cpp
    ${G}/vulkan/command_processor.cpp
    ${G}/vulkan/pipeline_cache.cpp
    ${G}/vulkan/primitive_processor.cpp
    ${G}/vulkan/render_target_cache.cpp
    ${G}/vulkan/shader.cpp
    ${G}/vulkan/shared_memory.cpp
    ${G}/vulkan/texture_cache.cpp
)
# The release plugin's install name and rpaths: no build-machine folders, and the runtime is found beside it.
set_target_properties(rexgpu-xenos PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    INSTALL_NAME_DIR "@rpath"
    BUILD_WITH_INSTALL_NAME_DIR ON
    INSTALL_RPATH "@loader_path;@loader_path/../lib"
    BUILD_WITH_INSTALL_RPATH ON
)
# glslang's headers come from its submodule; its libraries from the SDK.
target_include_directories(rexgpu-xenos PRIVATE
    "${REXGLUE_SRC}/thirdparty/renderdoc"
    "${REXGLUE_SRC}"
    "${REXGLUE_SRC}/thirdparty/glslang"
)
target_compile_options(rexgpu-xenos PRIVATE -Wno-unused-variable)
# The runtime's usage requirements, but only its dylib on the link line: spdlog and fmt must resolve to the copies
# librexruntime exports (one logger registry), not to the static archives in its link interface.
target_link_libraries(rexgpu-xenos PRIVATE
    $<COMPILE_ONLY:rex::runtime>
    $<TARGET_FILE:rex::runtime>
    rex::xxhash
    rex::SPIRV
    rex::glslang
    rex::MachineIndependent
    rex::GenericCodeGen
    rex::OSDependent
    rex::OGLCompiler
)
