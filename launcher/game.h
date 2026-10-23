// This game's profile: everything the launcher knows about the game it belongs to. The rest of launcher\ is the same
// for every game (recomp-tools' template copies it); game.h and game.cpp (the game's own texts) are each game's, with
// the art (launcher\art), the icon (res\icon.ico) and the version (res\version.h).

#pragma once

#include <array>
#include <cstdint>

#include "version.h"

namespace launcher::game {

// --- Names ---

// The game, as the texts name it ("the .iso file of %s").
inline constexpr const char* kName = "Kameo: Elements of Power";
// The recompiled executable and the launcher, side by side; the launcher's name is its window's title.
inline constexpr const char* kRecompExecutable = "KameoEOPRecomp.exe";
inline constexpr const char* kLauncherExecutable = "Kameo Launcher.exe";
inline constexpr const char* kLauncherName = "Kameo Launcher";
// The game's settings next to it: its base file (src/startup.cpp writes the defaults) and what the launcher saves.
inline constexpr const char* kConfigFile = "kameo.toml";
inline constexpr const char* kOptionsFile = "kameo_options.toml";
// The game's user data when its folder cannot be written: Documents\<this> (the SDK's default, the app's name).
inline constexpr const char* kDocumentsFolder = "kameo";
// res\version.h.
inline constexpr const char* kVersion = KAMEORECOMP_VERSION_STR;

// --- The look ---

// The title the art and the sidebar show: a small line above, the name below.
inline constexpr const char* kArtKicker = "KAMEO";
inline constexpr const char* kArtTitle = "Elements of Power";
// The mark in the logo beside it (the sidebar, the first run): an icon's SVG paths in a 24-unit box, stroked as the
// launcher's icons are (ui.cpp). The template's disc, until Gustavo's mockup gives the game its own.
inline constexpr const char* kBrandIcon =
    "<circle cx='12' cy='12' r='9'/><circle cx='12' cy='12' r='2.5'/><path d='M12 3a9 9 0 0 1 9 9'/>";
// Where the Play page crops its art (launcher\art\hero.svg) in a window of another shape: the point kept in view,
// as fractions of the art's width and height.
inline constexpr float kHeroFocusX = 0.7f;
inline constexpr float kHeroFocusY = 0.4f;

// --- The disc ---

// The disc's executable, at its root and in assets\ once extracted; extracted last, so that its presence marks a
// complete installation.
inline constexpr const char* kDiscExecutable = "default.xex";
// The disc this build was recompiled from (src/startup.cpp checks the same CRC at the game's start); its XEX allows
// the NTSC-U region only.
inline constexpr uint32_t kTitleId = 0x4D5307D2;
inline constexpr uint32_t kExecutableCrc32 = 0xE24BE720;
inline constexpr const char* kRegion = "USA";
// The game's files on that disc: the space the first run asks for before an ISO is chosen.
inline constexpr uint64_t kInstalledBytes = 5792303115;
// The game's frame: the Xenos path's scales and NativeXRenderer's internal resolutions are multiples of it.
inline constexpr int kFrameWidth = 1280;
inline constexpr int kFrameHeight = 720;

// --- Updates ---

// This project's releases (updater.cpp): GitHub's API for them, newest first, and their page. Empty: the launcher
// does not look for updates (the repository is private for now), and its Updates page only shows the version.
inline constexpr const char* kReleasesApi = "";
inline constexpr const char* kReleasesPage = "";

// --- The game's own options (a cvar each; empty: the game has no such option, and the launcher no row) ---

// NativeXRenderer draws this game (src/native_x_renderer_kameo.cpp): the Graphics page's renderer cards, its internal
// resolution, antialiasing and image format.
inline constexpr bool kHasNativeXRenderer = true;

// The Graphics page's frame rate row: kFrameRateCvar's choices, each a value of the cvar and the frame rate it
// gives, and the value without the key: src/framerate.cpp's 60 FPS, off by default.
struct FrameRate {
  int value;
  int fps;
};
inline constexpr const char* kFrameRateCvar = "kameo_60fps";
inline constexpr bool kFrameRateIsBool = true;
inline constexpr std::array<FrameRate, 2> kFrameRates{{{0, 30}, {1, 60}}};
inline constexpr int kFrameRateDefault = 0;

// The Xenos path's antialiasing cannot be turned off in this game.
inline constexpr const char* kNoMsaaCvar = "";
inline constexpr const char* kMsaaLabel = "";

// The game's cameras widened for a screen wider than 16:9 (src/widescreen.cpp), set with the Video page's image
// format; the Xenos path then stretches its image over the screen (present_letterbox = false).
inline constexpr const char* kUltrawideCvar = "kameo_ultrawide";

// The languages the game files have (one folder each), each in its own words; Spanish twice: Spain's (spanish\) and
// Latin America's (mexican\, kameo_latin_american_spanish, src/language.cpp), which Windows' regional format picks
// between until one is chosen. Korean and Traditional Chinese have text but no voices on the disc.
struct Language {
  uint32_t id;
  const char* name;
  const char* variant_cvar;  // null: none
  const char* locales;       // null: any locale of the language
};
inline constexpr std::array<Language, 9> kLanguages{{
    {3, "Deutsch", nullptr, nullptr},
    {1, "English", nullptr, nullptr},
    {5, "Español (España)", nullptr, "es-ES"},
    {5, "Español (Latinoamérica)", "kameo_latin_american_spanish", "es-"},
    {4, "Français", nullptr, nullptr},
    {6, "Italiano", nullptr, nullptr},
    {2, "日本語", nullptr, nullptr},
    {7, "한국어", nullptr, nullptr},
    {8, "繁體中文", nullptr, nullptr},
}};

// Switches of the game's own (src/), each a boolean cvar with a row in the Game page; their titles and hints are
// game.cpp's (GameTexts::toggles, in this order): the in-engine cinematics without their black bars
// (src/cinematics.cpp).
struct Toggle {
  const char* cvar;
  bool default_value;
};
inline constexpr std::array<Toggle, 1> kToggles{{{"kameo_no_letterbox", false}}};

// The Settings group has a Game page (page_game.cpp) when the game has a language row or switches.
inline constexpr bool kHasGamePage = !kLanguages.empty() || !kToggles.empty();

// --- Texts of the game's own (game.cpp) ---

struct GameTexts {
  // The first run's welcome, in the game's spirit; null: the launcher's own (texts.cpp).
  const char* welcome_title;
  const char* welcome_text;
  // kToggles' rows: a title and a hint each.
  std::array<const char*, kToggles.size() * 2> toggles;
};
// In the language of the launcher's texts (an XLanguage); missing ones fall back to English.
const GameTexts& GameTextsFor(uint32_t language);

}  // namespace launcher::game
