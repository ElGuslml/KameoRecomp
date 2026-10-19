# KameoRecomp - static recompilation of Kameo: Elements of Power (Xbox 360), built on the ReXGlue SDK,
# with NativeXRenderer (native_x_renderer/, a submodule) as an alternative renderer.
#
# The SDK is the thirdparty/rexglue-sdk git submodule (official repository, pinned to v0.10.0);
# the fixes in patches/ are applied to it at configure time (cmake/apply_sdk_patches.cmake).
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(kameo LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Same CPU baseline as the official SDK builds (its win-amd64 preset): x86-64-v2, i.e. SSE4.2 and
# POPCNT on top of the SSSE3/SSE4.1 the SDK code requires. Not -march=native, so the binaries run on
# any x86-64-v2 CPU.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
    add_compile_options(-march=x86-64-v2)
endif()

# Build the SDK from the submodule, with our patches.
set(REXSDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/rexglue-sdk" CACHE PATH
    "Path to the rexglue-sdk source tree" FORCE)
include(cmake/apply_sdk_patches.cmake)

# Both graphics backends: D3D12 and Vulkan.
if(WIN32)
    set(REXGLUE_USE_D3D12 ON CACHE BOOL "Enable the D3D12 graphics backend")
endif()
set(REXGLUE_USE_VULKAN ON CACHE BOOL "Enable the Vulkan graphics backend")

include(generated/rexglue.cmake)

# The first build of a fresh tree runs the codegen, which writes generated/default/sources.cmake;
# CMake has to configure again to pick up the generated sources (otherwise the link fails with
# PPCImageConfig). A marker file holds the sources.cmake this configure saw and is a configure
# dependency; it is refreshed with copy_if_different after the codegen, so it only changes (and
# triggers a new configure) when the list of generated sources changes. Ninja brings configure
# dependencies up to date before building anything else, so the first build runs the codegen,
# configures again and goes on within the same command. (The codegen stamp itself cannot be the
# dependency: the codegen touches it on every run.)
set(KAMEO_CODEGEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/generated/default")
set(KAMEO_SOURCES_MARKER "${KAMEO_CODEGEN_DIR}/sources.cmake.configured")
if(EXISTS "${KAMEO_CODEGEN_DIR}/sources.cmake")
    file(COPY_FILE "${KAMEO_CODEGEN_DIR}/sources.cmake" "${KAMEO_SOURCES_MARKER}" ONLY_IF_DIFFERENT)
elseif(NOT EXISTS "${KAMEO_SOURCES_MARKER}")
    file(WRITE "${KAMEO_SOURCES_MARKER}" "")
endif()
add_custom_command(
    OUTPUT "${KAMEO_SOURCES_MARKER}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${KAMEO_CODEGEN_DIR}/sources.cmake" "${KAMEO_SOURCES_MARKER}"
    DEPENDS "${KAMEO_CODEGEN_DIR}/codegen.build.stamp"
    COMMENT "Checking the list of generated sources"
    VERBATIM)
add_custom_target(kameo_sources_marker DEPENDS "${KAMEO_SOURCES_MARKER}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${KAMEO_SOURCES_MARKER}")

# Sources
set(KAMEO_SOURCES
    src/main.cpp
    src/bink_io.cpp
    src/cinematics.cpp
    src/framerate.cpp
    src/gpu_wait.cpp
    src/language.cpp
    src/native_x_renderer_kameo.cpp
    src/options.cpp
    src/startup.cpp
    src/widescreen.cpp
)

if(WIN32)
    add_executable(kameo WIN32 ${KAMEO_SOURCES})
else()
    add_executable(kameo ${KAMEO_SOURCES})
endif()

rexglue_setup_target(kameo GPU_PLUGINS xenos)
add_dependencies(kameo kameo_sources_marker)

# NativeXRenderer: the native renderer (native_x_renderer/README.md). The game side (hooks, profile) is in src/.
add_subdirectory(native_x_renderer)
target_link_libraries(kameo PRIVATE native_x_renderer)
# NVIDIA's DLSS runtime next to the executable.
native_x_renderer_copy_runtime(kameo)

# The target and the app keep the name "kameo" (kameo.toml, logs\kameo_NNN.log); only the
# executable gets a descriptive name.
set_target_properties(kameo PROPERTIES OUTPUT_NAME "kameorecomp")

# With the SDK built from source, rexruntime does not pass the Dear ImGui include directory on to the
# host target (the installed SDK ships imgui.h in include/); rex_app.cpp and kameo_app.h need it. The
# symbols still come from rexruntime.dll.
target_include_directories(kameo PRIVATE "${REXSDK_DIR}/thirdparty/imgui")

# Game files: assets/ next to kameorecomp.exe (release layout) or, if absent, the repository's
# assets/.
target_compile_definitions(kameo PRIVATE
    KAMEO_DEFAULT_GAME_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/assets")

# Kameo Launcher: game options in a separate executable (Win32, no SDK) so that kameorecomp.exe
# starts right away. It writes kameo_options.toml next to kameorecomp.exe and starts it on "Play".
if(WIN32)
    add_executable(kameo_launcher WIN32 launcher/kameo_launcher.cpp)
    set_target_properties(kameo_launcher PROPERTIES OUTPUT_NAME "Kameo Launcher")
    target_compile_definitions(kameo_launcher PRIVATE UNICODE _UNICODE NOMINMAX)
    target_link_libraries(kameo_launcher PRIVATE comctl32 shcore)
endif()

# Windows resources: file properties (res/version.h) and the icon, res/icon.ico, when it exists. The
# executable's first icon is also the one SDL and the launcher use for their windows.
if(WIN32)
    target_sources(kameo PRIVATE res/game.rc)
    target_sources(kameo_launcher PRIVATE res/launcher.rc)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/res/icon.ico")
        target_compile_definitions(kameo PRIVATE KAMEORECOMP_HAS_ICON)
        target_compile_definitions(kameo_launcher PRIVATE KAMEORECOMP_HAS_ICON)
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/res/icon.ico")
    endif()
endif()
