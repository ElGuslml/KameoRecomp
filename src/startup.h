// kameo - startup: default configuration and game files.

#pragma once

#include <filesystem>

namespace kameo {

// Game files folder when --game_data_root is not given: assets/ next to KameoEOPRecomp.exe (release
// layout) if it holds the game, otherwise the repository's assets/ (KAMEO_DEFAULT_GAME_ROOT, set in
// CMake); if neither does, assets/ next to the executable, so the "files not found" message names
// that folder.
std::filesystem::path DefaultGameRoot();

// User data (profile, saves, achievements, shader cache) in userdata\ next to the executable, so the
// whole game lives in one folder. Left as the SDK default (Documents\kameo) when --user_data_root
// was given or that folder cannot be written (for example under Program Files).
// user_data_root / cache_root: the SDK's defaults on entry (rex::PathConfig), updated in place.
void ConfigureUserData(std::filesystem::path& user_data_root, std::filesystem::path& cache_root);

// The launchers' profile the game runs with (RecompLauncher's profile_store.h): the current one's folder and name
// as the SDK's profile_root and profile_name (its saves, settings and achievements there), unless --profile_root
// was given. None while this installation has saves from before the profiles that the launcher has not copied yet:
// the game then keeps them in user_data_root, as before.
void ConfigureProfile(const std::filesystem::path& user_data_root);

// Writes config_path (kameo.toml) with the built-in defaults if it does not exist: the settings
// this game needs must not depend on a file that can go missing.
void EnsureDefaultConfig(const std::filesystem::path& config_path);

// Checks that game_root holds the default.xex the code was recompiled from. Missing file: error
// message, returns false (quit). Different version: warning, the user decides whether to go on.
bool CheckGameFiles(const std::filesystem::path& game_root);

}  // namespace kameo
