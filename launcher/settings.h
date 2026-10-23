// The launcher's settings: the game's base settings and the options file (game::kConfigFile, game::kOptionsFile),
// the monitors, and the resolutions the game and NativeXRenderer will use. Ported from the Win32 launcher it
// replaces; the file it writes keeps that launcher's keys, which the game loads at startup (src/options.cpp),
// plus the keyboard's, and the game's own options (game.h).

#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "game.h"

namespace launcher {

// The game always renders its frame (game.h; its Direct3D presentation parameters are hard-coded; the console
// video mode only drives the output scaler): resolution is raised with the SDK render scale
// (draw_resolution_scale_x/y, integers, per axis), or by NativeXRenderer at any size.
constexpr int kGuestWidth = game::kFrameWidth;
constexpr int kGuestHeight = game::kFrameHeight;
constexpr int kMaxScale = 3;

using Size = std::pair<int, int>;  // width, height

enum Display : int { kFullscreen, kBorderless, kWindow };

// NativeXRenderer's antialiasing (native_x_renderer_antialiasing): SMAA or FXAA on the finished scene (D41),
// DLAA with NVIDIA DLSS before the post-processing (D47; SMAA where DLSS is not available), or off.
enum NativeAntialiasing : int { kSmaa, kDlaa, kFxaa, kAaOff, kNativeAntialiasingCount };
inline constexpr const char* kNativeAntialiasingNames[] = {"smaa", "dlaa", "fxaa", "off"};

// The DLSS modes (NativeXRenderer's D49) draw the 3D view at a lower resolution and upscale it. They are not
// finished, and the launcher does not offer them (D53): the options file can still name one, and the row then shows it,
// marked experimental, so that saving keeps it.
struct ExperimentalAntialiasing {
  const char* name;
  const char* label;
};
inline constexpr ExperimentalAntialiasing kExperimentalAntialiasing[] = {
    {"dlss_quality", "DLSS Quality"},
    {"dlss_balanced", "DLSS Balanced"},
    {"dlss_performance", "DLSS Performance"},
    {"dlss_ultra_performance", "DLSS Ultra Performance"}};

// The keyboard's controls: the SDK's keybind_* cvars (thirdparty/rexglue-sdk/src/input/mnk), in the Keyboard
// page's order. Each is a comma-separated list of key names; the page edits the first two (primary and
// secondary) and keeps any others.
enum class Bind : int {
  kLStickUp,
  kLStickDown,
  kLStickLeft,
  kLStickRight,
  kLStickPress,
  kRStickUp,
  kRStickDown,
  kRStickLeft,
  kRStickRight,
  kRStickPress,
  kA,
  kB,
  kX,
  kY,
  kLeftShoulder,
  kRightShoulder,
  kLeftTrigger,
  kRightTrigger,
  kDpadUp,
  kDpadDown,
  kDpadLeft,
  kDpadRight,
  kStart,
  kBack,
  kCount
};

struct BindInfo {
  const char* cvar;
  const char* sdk_default;  // the SDK's default, as REXCVAR_DEFINE_STRING gives it
};
extern const BindInfo kBinds[int(Bind::kCount)];

struct Options {
  int monitor = 1;  // cvar monitor: 1 = primary, in SDL order
  int display = kBorderless;
  Size window{kGuestWidth, kGuestHeight};  // window size in physical pixels
  int scale = 0;                           // 0 = automatic; 1..kMaxScale = chosen
  int frame_rate = game::kFrameRateDefault;  // game::kFrameRateCvar's value (a game::kFrameRates value)
  bool no_msaa = false;                    // game::kNoMsaaCvar (src/msaa.cpp)
  int anisotropic = 5;                     // anisotropic_override (SDK): 5 = 16x, 4 = 8x, 3 = 4x, 2 = 2x, 0 = off
  bool vulkan = false;                     // gpu_backend = "vulkan" (otherwise "d3d12")
  bool native = game::kHasNativeXRenderer;  // native_x_renderer: NativeXRenderer draws instead of Xenos (D56)
  // native_x_renderer_antialiasing: a NativeAntialiasing, or kNativeAntialiasingCount for the experimental mode
  // the options file named (native_aa_experimental, an index of kExperimentalAntialiasing; -1: none).
  int native_aa = kSmaa;
  int native_aa_experimental = -1;
  bool native_fill = true;  // native_x_renderer_fill_screen: fill an output that is not 16:9, or 16:9 with bars
  int native_height = 0;    // native_x_renderer_resolution_scale × the frame's height: the internal resolution's
                            // height; 0 = automatic
  // native_x_renderer_shadow_scale (NativeXRenderer's D92): 0 = the shadow maps like everything else, 1 = at least
  // twice the game's resolution (2), 2 = at least four times (4).
  int shadow_quality = 0;
  // The game's own options (game.h): its language (an index of game::kLanguages: user_language and its variant
  // cvar) and its switches. game::kUltrawideCvar follows native_fill (the image format).
  int language = 0;
  std::array<bool, game::kToggles.size()> toggles{};

  // Keyboard and mouse: the SDK's controller emulation (mnk_mode, mnk_mouse, mnk_sensitivity, keybind_*).
  bool mnk = false;
  bool mnk_mouse = false;
  float mnk_sensitivity = 1.0f;
  std::array<std::string, int(Bind::kCount)> binds;

  // The controllers' buttons moved to other Xbox 360 controls: the SDK's gamepad_remap (recomp change 36),
  // edited by the Gamepad page (page_gamepad.cpp).
  std::string gamepad_remap;

  // The launcher's own.
  bool check_updates = true;    // launcher_check_updates
  int64_t last_update_check = 0;  // launcher_last_update_check: when a check last answered (Unix time); 0 = never
  bool ui_fullscreen = true;    // launcher_ui_fullscreen: the launcher's window (fullscreen by default)
  bool ui_maximized = false;    // launcher_ui_maximized
  Size ui_size{0, 0};           // launcher_ui_width/height, in pixels at 100 % scaling; 0 = the default
};

// The defaults of each page's "Reset" (the game's own, src/startup.cpp; the SDK's for the keyboard).
void ResetVideo(Options& o);
void ResetGraphics(Options& o);
void ResetGame(Options& o);
void ResetKeyboard(Options& o);

// --- The game's base settings and the options file ---

// Minimal TOML reader ("key = value"; [x] tables are flattened as x_key, like the SDK does).
using Config = std::map<std::string, std::string>;
void ReadToml(const std::filesystem::path& path, Config& config);

// `primary_dpi`: the SDK converts window_width/height with the primary monitor's DPI.
Options OptionsFromConfig(const Config& c, unsigned primary_dpi);

// --- Monitors ---

struct Monitor {
  std::string name;  // monitor name (EDID), UTF-8; empty if Windows does not provide it
  bool primary = false;
  Size size{0, 0};  // current mode, in physical pixels
  int work_left = 0, work_top = 0, work_right = 0, work_bottom = 0;  // work area (without the taskbar)
  unsigned dpi = 96;
  std::vector<Size> modes;  // 32-bit modes, unique, largest first
  int refresh = 60;          // the current mode's refresh rate, Hz
};

// Monitors in SDL order (the one the monitor cvar uses): the primary first, then the rest in
// EnumDisplayMonitors order (SDL_windowsmodes.c, WIN_AddDisplays).
std::vector<Monitor> EnumerateMonitors();

// The frame rates the monitor reaches (game::kFrameRates, those above 60 only up to its refresh rate).
std::vector<game::FrameRate> FrameRates(const Monitor& monitor);
// The Windows display language's entry of game::kLanguages, or 0.
int SystemGameLanguage();

// Window sizes that fit (frame included) in the monitor work area: its modes from 1280x720 up,
// 1280x720 always, plus keep (the saved size) if it is not in the list.
std::vector<Size> WindowSizes(const Monitor& m, Size keep);

// 16:9 aspect ratio within 1 % (1366x768 and 1360x768 count).
bool Is16x9(Size size);

// Render scale per axis: the smallest one that covers (with 10 % tolerance) the 16:9 image inside
// the output (the presenter letterboxes other aspect ratios).
Size AutoScale(Size output);

// NativeXRenderer's render size (D37, D43, D54), as the renderer computes it from its swap chain.
Size NativeRenderSize(Size output, bool fill, int height);

// The internal resolutions NativeXRenderer offers, as heights (D54): 0 (automatic), then lower ones.
std::vector<int> NativeHeights(Size output, bool fill, int keep);

// --- The settings being edited ---

struct Settings {
  Options o;
  std::vector<Monitor> monitors;
  std::vector<Size> window_sizes;  // of the chosen monitor (physical pixels)
  unsigned primary_dpi = 96;
  bool has_vulkan = false;  // vulkan-1.dll present (ships with the video driver)
  std::filesystem::path options_path;

  const Monitor& CurrentMonitor() const;
  Size OutputSize() const;
  Size EffectiveScale() const;
  // After the monitor changed: its window sizes, and a size it does not offer becomes 1280x720.
  void MonitorChanged();
  // A frame rate the monitor does not reach goes back to the game's default.
  void FitFrameRate();
};

// Reads the game's base settings and the options file next to the executable, and what the machine offers.
Settings LoadSettings(const std::filesystem::path& exe_dir);

// Writes the options file. False if it could not be written.
bool SaveOptions(const Settings& s);

}  // namespace launcher
