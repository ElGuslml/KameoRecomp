// The game's achievements for the Achievements page and the Play card.
//
// They live in the game's executable, which the launcher does not read: the game writes them for launchers each
// time it starts (the SDK's achievement cache, rex/system/achievement_cache.h), next to the unlocks, in its user
// data folder (userdata\ next to it, or Documents\<game::kDocumentsFolder> when that cannot be written):
// - achievements\<TITLEID>.info.toml: every achievement in every language the game has, and its icon;
// - achievements\<TITLEID>\<image id>.png: the icons, the game's own;
// - achievements\<TITLEID>.toml: the unlocks, with their times.
// When the list is missing (the game has not run with this version yet, or was just installed), the game is asked
// to write it and quit (--achievements_cache_only): about a second, with no window.

#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace launcher {

struct Achievement {
  uint32_t id = 0;
  uint32_t gamerscore = 0;
  uint32_t flags = 0;
  // In the launcher's language, or the game's default one.
  std::string label;
  std::string description;
  std::string unachieved_description;
  uint64_t unlocked = 0;  // when it was unlocked (FILETIME, UTC); 0 = locked
  // The icon as RGBA pixels (straight alpha); empty if it has none.
  std::vector<uint8_t> icon;
  int icon_width = 0;
  int icon_height = 0;

  // Hidden until unlocked: the game's "show unachieved" flag (0x8) is clear.
  bool secret() const { return (flags & 0x8) == 0; }
};

struct Achievements {
  std::vector<Achievement> items;  // in the game's order
  int unlocked = 0;
  uint32_t gamerscore = 0;
  uint32_t total_gamerscore = 0;
};

// Reads the list and the unlocks for the language (an XLanguage). nullopt when the game has not written the list.
// Slow (files, icons): for a thread.
std::optional<Achievements> LoadAchievements(const std::filesystem::path& exe_dir, uint32_t language);

// Starts the game to write the list, without a window, and waits for it (at most 30 s). False if it could not run.
bool WriteAchievementList(const std::filesystem::path& exe_dir);

// A FILETIME as a date in local time, in the language's order ("15/10/2026", "10/15/2026", "15.10.2026").
std::string FormatDate(uint64_t filetime, uint32_t language);

}  // namespace launcher
