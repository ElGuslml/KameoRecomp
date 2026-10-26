// This game's profile: everything the launcher knows about the game it belongs to. The launcher's code is the same
// for every game (the RecompLauncher submodule, recomp_launcher\, with game_types.h for the types below); game.h,
// game.cpp (the game's own texts) and palette.h (its colors) are each game's, with the art (launcher\art), the icon
// (res\icon.ico), the resource script (res\launcher.rc) and the version (res\version.h).

#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "game_types.h"
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
inline constexpr const char* kArtKicker = "ELEMENTS OF POWER";
inline constexpr const char* kArtTitle = "KAMEO";
// The mark in the logo beside it (the sidebar, the first run): an icon's SVG paths in a 24-unit box, stroked as the
// launcher's icons are (ui.cpp). A leaf (Gustavo's mockup).
inline constexpr const char* kBrandIcon = "<path d='M5 19c0-8 6-14 14-14 0 8-6 14-14 14z'/><path d='M5 19l8-8'/>";
// Where the Play page crops its art (launcher\art\hero.svg) in a window of another shape: the point kept in view,
// as fractions of the art's width and height (the elemental orb, at 470, 210 of 1000 x 524).
inline constexpr float kHeroFocusX = 0.47f;
inline constexpr float kHeroFocusY = 0.40f;
// The logo's mark on a disc instead of a rounded square.
inline constexpr bool kBrandRound = true;
// The first run's art panel: the game's name in a dark card over the art, with the tagline, instead of straight on
// the art with the tagline at its foot.
inline constexpr bool kInstallTitleCard = true;
// The sidebar's name beside the logo, and the words before "LAUNCHER" under it; null: the art's title (kArtTitle) and
// "LAUNCHER" alone.
inline constexpr const char* kSidebarTitle = nullptr;
inline constexpr const char* kSidebarSubtitle = nullptr;
// The Play page's name and buttons in a black bar across the foot of the art, under a line of accent (the first run's
// title card gets the same line), instead of straight on the art.
inline constexpr bool kHeroTitleBar = false;
// The titles in capitals: the pages', the Play page's name, the sidebar's and the first run's.
inline constexpr bool kUppercaseTitles = false;

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
// does not look for updates (a private repository), and its Updates page only shows the version.
inline constexpr const char* kReleasesApi = "https://api.github.com/repos/ElGuslml/KameoRecomp/releases?per_page=20";
inline constexpr const char* kReleasesPage = "https://github.com/ElGuslml/KameoRecomp/releases";

// --- The game's own options (a cvar each; empty: the game has no such option, and the launcher no row) ---

// NativeXRenderer draws this game (src/native_x_renderer_kameo.cpp): the Graphics page's renderer cards, its internal
// resolution, antialiasing and image format.
inline constexpr bool kHasNativeXRenderer = true;
// The renderer chosen until the player picks one (an options file without native_x_renderer): NativeXRenderer.
inline constexpr bool kNativeXRendererDefault = true;

// The Graphics page's shadow quality row: NativeXRenderer draws the game's shadow maps at a scale of their own
// (native_x_renderer_shadow_scale, its D92): at least twice or four times the game's resolution. Only where it
// finds them: the depth passes with no color target that are not of the game's output size.
// Its shadow maps: 768x768 and 1024x1024 depth maps, drawn with no color target.
inline constexpr bool kShadowQuality = kHasNativeXRenderer && true;

// The Graphics page's frame rate row: kFrameRateCvar's choices, each a value of the cvar and the frame rate it
// gives, and the value without the key: src/framerate.cpp's kameo_frame_rate (src/options.cpp turns it into
// kameo_60fps and the vblank's rate), 30 by default as on the console. The rates above the monitor's are hidden.
inline constexpr const char* kFrameRateCvar = "kameo_frame_rate";
inline constexpr bool kFrameRateIsBool = false;
inline constexpr std::array<FrameRate, 7> kFrameRates{
    {{30, 30}, {60, 60}, {120, 120}, {144, 144}, {160, 160}, {165, 165}, {240, 240}}};
inline constexpr int kFrameRateDefault = 30;

// The Xenos path's antialiasing cannot be turned off in this game.
inline constexpr const char* kNoMsaaCvar = "";
inline constexpr const char* kMsaaLabel = "";

// The game's cameras widened for a screen wider than 16:9 (src/widescreen.cpp), set with the Video page's image
// format; the Xenos path then stretches its image over the screen (present_letterbox = false).
inline constexpr const char* kUltrawideCvar = "kameo_ultrawide";

// The languages the game files have (one folder each), each in its own words; Spanish twice: Spain's (spanish\) and
// Latin America's (mexican\, kameo_latin_american_spanish, src/language.cpp), which Windows' regional format picks
// between until one is chosen. Korean and Traditional Chinese have text but no voices on the disc.
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
inline constexpr std::array<Toggle, 1> kToggles{{{"kameo_no_letterbox", false}}};

// The Settings group has a Game page (page_game.cpp) when the game has a language row or switches.
inline constexpr bool kHasGamePage = !kLanguages.empty() || !kToggles.empty();

// --- Downloadable content ---

// The DLC page (page_dlc.cpp, dlc.h): Xbox 360 content packages, each installed by the game's --install_content tool
// mode (SDK change 44) into its user data's content folder. Off until checked with this game's content.
inline constexpr bool kHasDlcPage = false;

// --- The keyboard ---

// The Keyboard page's defaults where they are not the SDK's (game_types.h's KeyDefault); empty: the SDK's.
inline constexpr std::array<KeyDefault, 0> kKeyboardDefaults{};
// The Keyboard page's switch that makes some keys also tap a control (the SDK's mnk_tap_target); "": no switch.
inline constexpr const char* kKeyboardTapTarget = "";

// --- The controls in the game's words ---

// The game's functions (game_types.h's GameControl) in groups, in the launcher's language (game.cpp); empty: the pages
// show the Xbox 360 controls (texts.cpp).
const std::vector<GameControlGroup>& GameControlsFor(uint32_t language);

// --- Texts of the game's own (game.cpp) ---

struct GameTexts {
  // The first run's welcome, in the game's spirit; null: the launcher's own (texts.cpp).
  const char* welcome_title;
  const char* welcome_text;
  // kToggles' rows: a title and a hint each.
  std::array<const char*, kToggles.size() * 2> toggles;
  // The first run's last screen (the game installed), in the game's spirit; null: the launcher's own.
  const char* done_title = nullptr;
  const char* done_text = nullptr;
  // The DLC page: where the game shows what was installed; null: nothing said.
  const char* dlc_where = nullptr;
  // The first run's extraction screen (while it runs), in the game's spirit; null: the launcher's own.
  const char* extracting_title = nullptr;
  const char* extracting_text = nullptr;
  // The Gamepad page's name in the sidebar, its title and subtitle; null: the launcher's own ("Gamepad").
  const char* gamepad_nav = nullptr;
  const char* gamepad_title = nullptr;
  const char* gamepad_subtitle = nullptr;
  // The Keyboard and Gamepad pages' first column with GameControlsFor's functions; null: the launcher's own.
  const char* functions_column = nullptr;
  // The Keyboard page's switch for kKeyboardTapTarget.
  const char* keyboard_tap_title = nullptr;
  const char* keyboard_tap_hint = nullptr;
};
// In the language of the launcher's texts (an XLanguage); missing ones fall back to English.
const GameTexts& GameTextsFor(uint32_t language);

}  // namespace launcher::game
