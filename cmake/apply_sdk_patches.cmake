# Applies the patches in patches/ (a git format-patch series, in file name order) to the ReXGlue SDK
# submodule (REXSDK_DIR) at configure time.
#
# - Pristine submodule (no changes to tracked files): every patch is applied in order and a stamp with
#   the series hash is written to the submodule's git directory.
# - Submodule already patched with this same series (stamp matches): nothing to do.
# - Anything else (the patches changed, or files in the submodule were edited): stop and explain how to
#   start over. Patches in a series depend on each other, so they cannot be checked one by one.

find_package(Git REQUIRED)

if(NOT EXISTS "${REXSDK_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "The ReXGlue SDK submodule is missing (${REXSDK_DIR}).\n"
        "Clone with --recursive, or run: git submodule update --init --recursive")
endif()

file(GLOB _rex_patches "${CMAKE_CURRENT_LIST_DIR}/../patches/*.patch")
list(SORT _rex_patches)
list(LENGTH _rex_patches _rex_patch_count)

set(_rex_series "")
foreach(_patch IN LISTS _rex_patches)
    file(SHA256 "${_patch}" _hash)
    string(APPEND _rex_series "${_hash}\n")
endforeach()
string(SHA256 _rex_series_hash "${_rex_series}")

execute_process(
    COMMAND "${GIT_EXECUTABLE}" rev-parse --git-dir
    WORKING_DIRECTORY "${REXSDK_DIR}"
    OUTPUT_VARIABLE _rex_git_dir
    OUTPUT_STRIP_TRAILING_WHITESPACE)
get_filename_component(_rex_git_dir "${_rex_git_dir}" ABSOLUTE BASE_DIR "${REXSDK_DIR}")
set(_rex_stamp "${_rex_git_dir}/kameorecomp-patches.stamp")

execute_process(
    COMMAND "${GIT_EXECUTABLE}" status --porcelain --untracked-files=no --ignore-submodules=all
    WORKING_DIRECTORY "${REXSDK_DIR}"
    OUTPUT_VARIABLE _rex_status)

set(_rex_stamp_hash "")
if(EXISTS "${_rex_stamp}")
    file(READ "${_rex_stamp}" _rex_stamp_hash)
    string(STRIP "${_rex_stamp_hash}" _rex_stamp_hash)
endif()

if(_rex_status STREQUAL "")
    foreach(_patch IN LISTS _rex_patches)
        get_filename_component(_name "${_patch}" NAME)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" apply "${_patch}"
            WORKING_DIRECTORY "${REXSDK_DIR}"
            RESULT_VARIABLE _result
            ERROR_VARIABLE _error)
        if(NOT _result EQUAL 0)
            message(FATAL_ERROR
                "[sdk-patches] ${_name} does not apply to ${REXSDK_DIR}:\n${_error}\n"
                "Is the submodule at the pinned commit? Run: git submodule update --init --recursive")
        endif()
    endforeach()
    file(WRITE "${_rex_stamp}" "${_rex_series_hash}\n")
    message(STATUS "[sdk-patches] applied ${_rex_patch_count} patches to ${REXSDK_DIR}")
elseif(_rex_stamp_hash STREQUAL _rex_series_hash)
    message(STATUS "[sdk-patches] ${_rex_patch_count} patches already applied")
else()
    message(FATAL_ERROR
        "[sdk-patches] ${REXSDK_DIR} has changes that do not match patches/ (the patches changed, or files "
        "in the submodule were edited).\n"
        "To start over from the pinned SDK with the current patches (this discards edits in the submodule):\n"
        "  git -C thirdparty/rexglue-sdk checkout -- .\n"
        "then configure again. To change the SDK, see \"Changing the SDK\" in CLAUDE.md.")
endif()
