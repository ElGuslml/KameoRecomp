// The launcher's settings (settings.h).

#include "settings.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellscalingapi.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iterator>

namespace launcher {

const BindInfo kBinds[int(Bind::kCount)] = {
    {"keybind_lstick_up", "W"},
    {"keybind_lstick_down", "S"},
    {"keybind_lstick_left", "A"},
    {"keybind_lstick_right", "D"},
    {"keybind_lstick_press", "F"},
    {"keybind_rstick_up", "Up"},
    {"keybind_rstick_down", "Down"},
    {"keybind_rstick_left", "Left"},
    {"keybind_rstick_right", "Right"},
    {"keybind_rstick_press", "K"},
    {"keybind_a", "Semicolon,Space"},
    {"keybind_b", "Quote,Backspace"},
    {"keybind_x", "L"},
    {"keybind_y", "P"},
    {"keybind_left_shoulder", "1"},
    {"keybind_right_shoulder", "3"},
    {"keybind_left_trigger", "Q,I"},
    {"keybind_right_trigger", "E,O"},
    {"keybind_dpad_up", "Shift+Up"},
    {"keybind_dpad_down", "Shift+Down"},
    {"keybind_dpad_left", "Shift+Left"},
    {"keybind_dpad_right", "Shift+Right"},
    {"keybind_start", "X,Return"},
    {"keybind_back", "Z,Tab"},
};

void ResetVideo(Options& o) {
  Options defaults;
  o.monitor = defaults.monitor;
  o.display = defaults.display;
  o.window = defaults.window;
  o.native_fill = defaults.native_fill;
  o.native_height = defaults.native_height;
  o.scale = defaults.scale;
}

void ResetGraphics(Options& o) {
  Options defaults;
  o.native = defaults.native;
  o.vulkan = defaults.vulkan;
  o.frame_rate = defaults.frame_rate;
  o.no_msaa = defaults.no_msaa;
  o.native_aa = defaults.native_aa;
  o.native_aa_experimental = defaults.native_aa_experimental;
  o.anisotropic = defaults.anisotropic;
}

void ResetGame(Options& o) {
  o.language = SystemGameLanguage();
  for (size_t i = 0; i < game::kToggles.size(); ++i) {
    o.toggles[i] = game::kToggles[i].default_value;
  }
}

std::vector<game::FrameRate> FrameRates(const Monitor& monitor) {
  std::vector<game::FrameRate> rates;
  for (const game::FrameRate& rate : game::kFrameRates) {
    if (rate.fps <= 60 || rate.fps <= monitor.refresh + 1) {
      rates.push_back(rate);
    }
  }
  return rates;
}

int SystemGameLanguage() {
  if (game::kLanguages.empty()) {
    return 0;
  }
  uint32_t id = 1;
  switch (PRIMARYLANGID(GetUserDefaultUILanguage())) {
    case LANG_JAPANESE: id = 2; break;
    case LANG_GERMAN: id = 3; break;
    case LANG_FRENCH: id = 4; break;
    case LANG_SPANISH: id = 5; break;
    case LANG_ITALIAN: id = 6; break;
    case LANG_KOREAN: id = 7; break;
    case LANG_CHINESE:
      id = SUBLANGID(GetUserDefaultUILanguage()) == SUBLANG_CHINESE_SIMPLIFIED ? 10 : 8;
      break;
    case LANG_PORTUGUESE: id = 9; break;
    case LANG_POLISH: id = 11; break;
    case LANG_RUSSIAN: id = 12; break;
    default: break;
  }
  // The user's locale ("es-MX") picks between two versions of the language.
  wchar_t wide[LOCALE_NAME_MAX_LENGTH] = {};
  GetUserDefaultLocaleName(wide, LOCALE_NAME_MAX_LENGTH);
  std::string locale;
  for (const wchar_t* c = wide; *c; ++c) locale += char(*c);
  // The longest `locales` prefix that matches wins; an entry without one comes next, then one whose prefix does not
  // match (the first of those).
  int best = -1;
  int best_score = 0;
  for (size_t i = 0; i < game::kLanguages.size(); ++i) {
    const game::Language& l = game::kLanguages[i];
    if (l.id != id) continue;
    int score = !l.locales ? 0 : locale.rfind(l.locales, 0) == 0 ? int(std::strlen(l.locales)) : -1;
    if (best < 0 || score > best_score) {
      best = int(i);
      best_score = score;
    }
  }
  if (best >= 0) {
    return best;
  }
  // English, or the first.
  for (size_t i = 0; i < game::kLanguages.size(); ++i) {
    if (game::kLanguages[i].id == 1) return int(i);
  }
  return 0;
}

void ResetKeyboard(Options& o) {
  for (int i = 0; i < int(Bind::kCount); ++i) {
    o.binds[i] = kBinds[i].sdk_default;
  }
  o.mnk_mouse = false;
  o.mnk_sensitivity = 1.0f;
}

// --- The game's base settings and the options file ---

namespace {

std::string Trim(const std::string& s) {
  size_t begin = s.find_first_not_of(" \t\r\n");
  size_t end = s.find_last_not_of(" \t\r\n");
  return begin == std::string::npos ? std::string() : s.substr(begin, end - begin + 1);
}

std::string GetString(const Config& c, const std::string& key) {
  auto it = c.find(key);
  return it != c.end() ? it->second : std::string();
}

int GetInt(const Config& c, const std::string& key, int fallback) {
  std::string s = GetString(c, key);
  char* end = nullptr;
  long value = std::strtol(s.c_str(), &end, 10);
  return end != s.c_str() ? int(value) : fallback;
}

double GetDouble(const Config& c, const std::string& key, double fallback) {
  std::string s = GetString(c, key);
  char* end = nullptr;
  double value = std::strtod(s.c_str(), &end);
  return end != s.c_str() ? value : fallback;
}

bool GetBool(const Config& c, const std::string& key, bool fallback) {
  std::string s = GetString(c, key);
  return s == "true" ? true : s == "false" ? false : fallback;
}

std::string Utf8(const wchar_t* text) {
  int length = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
  if (length <= 1) {
    return {};
  }
  std::string out(size_t(length - 1), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text, -1, out.data(), length, nullptr, nullptr);
  return out;
}

}  // namespace

void ReadToml(const std::filesystem::path& path, Config& config) {
  std::ifstream f(path);
  std::string line;
  std::string table;
  bool first_line = true;
  while (std::getline(f, line)) {
    if (first_line && line.starts_with("\xEF\xBB\xBF")) {  // UTF-8 BOM
      line.erase(0, 3);
    }
    first_line = false;
    bool quoted = false;
    for (size_t i = 0; i < line.size(); ++i) {
      if (line[i] == '"') {
        quoted = !quoted;
      } else if (line[i] == '#' && !quoted) {
        line.resize(i);
        break;
      }
    }
    line = Trim(line);
    if (line.empty()) {
      continue;
    }
    if (line.front() == '[' && line.back() == ']') {
      table = Trim(line.substr(1, line.size() - 2));
      continue;
    }
    size_t eq = line.find('=');
    if (eq == std::string::npos) {
      continue;
    }
    std::string key = Trim(line.substr(0, eq));
    std::string value = Trim(line.substr(eq + 1));
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
      value = value.substr(1, value.size() - 2);
    }
    config[table.empty() ? key : table + "_" + key] = value;
  }
}

Options OptionsFromConfig(const Config& c, unsigned primary_dpi) {
  Options o;
  o.monitor = std::max(1, GetInt(c, "monitor", 1));
  if (!GetBool(c, "fullscreen", true)) {
    o.display = kWindow;
  } else {
    // fullscreen_exclusive (the SDK's): switch the display mode; otherwise fullscreen is borderless at the
    // desktop mode, the SDK's default. An options file from before it has fullscreen_borderless_window.
    bool exclusive = !GetString(c, "fullscreen_exclusive").empty()
                         ? GetBool(c, "fullscreen_exclusive", false)
                         : !GetString(c, "fullscreen_borderless_window").empty() &&
                               !GetBool(c, "fullscreen_borderless_window", false);
    o.display = exclusive ? kFullscreen : kBorderless;
  }
  // launcher_window_*: the chosen size, kept even when playing fullscreen (see SaveOptions).
  int window_width = GetInt(c, "launcher_window_width", GetInt(c, "window_width", 0));
  int window_height = GetInt(c, "launcher_window_height", GetInt(c, "window_height", 0));
  if (window_width > 0 && window_height > 0) {
    o.window = {MulDiv(window_width, int(primary_dpi), 96), MulDiv(window_height, int(primary_dpi), 96)};
  }
  o.vulkan = GetString(c, "gpu_backend") == "vulkan";
  if (*game::kFrameRateCvar) {
    int value = game::kFrameRateIsBool
                    ? int(GetBool(c, game::kFrameRateCvar, game::kFrameRateDefault != 0))
                    : int(std::lround(std::strtod(GetString(c, game::kFrameRateCvar).c_str(), nullptr)));
    bool known = std::any_of(game::kFrameRates.begin(), game::kFrameRates.end(),
                             [value](const game::FrameRate& rate) { return rate.value == value; });
    o.frame_rate = known ? value : game::kFrameRateDefault;
  }
  o.no_msaa = *game::kNoMsaaCvar && GetBool(c, game::kNoMsaaCvar, false);
  // The language: the entry of user_language whose variant cvar is set, or the one without; the Windows display
  // language's until one is chosen.
  o.language = -1;
  if (uint32_t id = uint32_t(GetInt(c, "user_language", 0)); id != 0 && !GetString(c, "user_language").empty()) {
    for (size_t i = 0; i < game::kLanguages.size(); ++i) {
      const game::Language& l = game::kLanguages[i];
      if (l.id != id) continue;
      if (l.variant_cvar ? GetBool(c, l.variant_cvar, false) : o.language < 0) {
        o.language = int(i);
      }
    }
  }
  if (o.language < 0) {
    o.language = SystemGameLanguage();
  }
  for (size_t i = 0; i < game::kToggles.size(); ++i) {
    o.toggles[i] = GetBool(c, game::kToggles[i].cvar, game::kToggles[i].default_value);
  }
  o.native = game::kHasNativeXRenderer && GetBool(c, "native_x_renderer", true);  // the default (D56)
  o.native_fill = GetBool(c, "native_x_renderer_fill_screen", true);
  double native_scale = std::strtod(GetString(c, "native_x_renderer_resolution_scale").c_str(), nullptr);
  o.native_height = native_scale > 0.0 ? int(std::lround(native_scale * kGuestHeight)) : 0;
  std::string native_aa = GetString(c, "native_x_renderer_antialiasing");
  for (int i = 0; i < kNativeAntialiasingCount; ++i) {
    if (native_aa == kNativeAntialiasingNames[i]) o.native_aa = i;
  }
  for (int i = 0; i < int(std::size(kExperimentalAntialiasing)); ++i) {
    if (native_aa == kExperimentalAntialiasing[i].name) {
      o.native_aa_experimental = i;
      o.native_aa = kNativeAntialiasingCount;
    }
  }
  // Without the key, the game's default (src/startup.cpp adds it to its base settings at the next start): 16x.
  // A value the row does not offer: -1 (only what the game asks: 4x on some textures) shows as 4x, 1 (1x)
  // as off.
  int anisotropic = GetInt(c, "anisotropic_override", 5);
  o.anisotropic = anisotropic < 0 ? 3 : anisotropic == 1 ? 0 : std::min(anisotropic, 5);
  if (GetString(c, "launcher_render_scale") == "auto") {
    o.scale = 0;
  } else if (int scale = GetInt(c, "launcher_render_scale", 0); scale >= 1) {
    o.scale = std::min(scale, kMaxScale);
  } else {
    // Without that key (the base settings edited by hand): the square scale that was chosen there.
    int x = GetInt(c, "draw_resolution_scale_x", 0);
    int y = GetInt(c, "draw_resolution_scale_y", 0);
    o.scale = x >= 1 && x == y ? std::min(x, kMaxScale) : 0;
  }

  o.mnk = GetBool(c, "mnk_mode", false);
  o.mnk_mouse = GetBool(c, "mnk_mouse", false);
  o.mnk_sensitivity = float(std::clamp(GetDouble(c, "mnk_sensitivity", 1.0), 0.01, 10.0));
  for (int i = 0; i < int(Bind::kCount); ++i) {
    auto it = c.find(kBinds[i].cvar);
    o.binds[i] = it != c.end() ? it->second : kBinds[i].sdk_default;
  }

  o.gamepad_remap = GetString(c, "gamepad_remap");

  o.check_updates = GetBool(c, "launcher_check_updates", true);
  o.last_update_check = std::strtoll(GetString(c, "launcher_last_update_check").c_str(), nullptr, 10);
  o.ui_fullscreen = GetBool(c, "launcher_ui_fullscreen", true);
  o.ui_maximized = GetBool(c, "launcher_ui_maximized", false);
  o.ui_size = {GetInt(c, "launcher_ui_width", 0), GetInt(c, "launcher_ui_height", 0)};
  return o;
}

// --- Monitors ---

namespace {

// Name of each monitor by GDI device (\\.\DISPLAYn), obtained the same way as SDL.
std::map<std::wstring, std::wstring> MonitorNames() {
  std::map<std::wstring, std::wstring> names;
  UINT32 path_count = 0;
  UINT32 mode_count = 0;
  if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &path_count, &mode_count) != ERROR_SUCCESS) {
    return names;
  }
  std::vector<DISPLAYCONFIG_PATH_INFO> paths(path_count);
  std::vector<DISPLAYCONFIG_MODE_INFO> modes(mode_count);
  if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &path_count, paths.data(), &mode_count, modes.data(), nullptr) !=
      ERROR_SUCCESS) {
    return names;
  }
  for (UINT32 i = 0; i < path_count; ++i) {
    DISPLAYCONFIG_SOURCE_DEVICE_NAME source = {};
    source.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
    source.header.size = sizeof(source);
    source.header.adapterId = paths[i].sourceInfo.adapterId;
    source.header.id = paths[i].sourceInfo.id;
    DISPLAYCONFIG_TARGET_DEVICE_NAME target = {};
    target.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
    target.header.size = sizeof(target);
    target.header.adapterId = paths[i].targetInfo.adapterId;
    target.header.id = paths[i].targetInfo.id;
    if (DisplayConfigGetDeviceInfo(&source.header) == ERROR_SUCCESS &&
        DisplayConfigGetDeviceInfo(&target.header) == ERROR_SUCCESS && target.monitorFriendlyDeviceName[0]) {
      names.emplace(source.viewGdiDeviceName, target.monitorFriendlyDeviceName);
    }
  }
  return names;
}

}  // namespace

std::vector<Monitor> EnumerateMonitors() {
  std::vector<HMONITOR> handles;
  EnumDisplayMonitors(
      nullptr, nullptr,
      [](HMONITOR handle, HDC, LPRECT, LPARAM param) -> BOOL {
        reinterpret_cast<std::vector<HMONITOR>*>(param)->push_back(handle);
        return TRUE;
      },
      LPARAM(&handles));
  std::map<std::wstring, std::wstring> names = MonitorNames();

  std::vector<Monitor> monitors;
  for (HMONITOR handle : handles) {
    MONITORINFOEXW info = {};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(handle, &info)) {
      continue;
    }
    Monitor m;
    m.primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
    m.work_left = info.rcWork.left;
    m.work_top = info.rcWork.top;
    m.work_right = info.rcWork.right;
    m.work_bottom = info.rcWork.bottom;
    m.size = {info.rcMonitor.right - info.rcMonitor.left, info.rcMonitor.bottom - info.rcMonitor.top};
    DEVMODEW current = {};
    current.dmSize = sizeof(current);
    if (EnumDisplaySettingsW(info.szDevice, ENUM_CURRENT_SETTINGS, &current)) {
      m.size = {int(current.dmPelsWidth), int(current.dmPelsHeight)};
      if (current.dmDisplayFrequency > 1) {
        m.refresh = int(current.dmDisplayFrequency);
      }
    }
    for (DWORD i = 0;; ++i) {
      DEVMODEW mode = {};
      mode.dmSize = sizeof(mode);
      if (!EnumDisplaySettingsW(info.szDevice, i, &mode)) {
        break;
      }
      Size size{int(mode.dmPelsWidth), int(mode.dmPelsHeight)};
      if (mode.dmBitsPerPel == 32 && std::find(m.modes.begin(), m.modes.end(), size) == m.modes.end()) {
        m.modes.push_back(size);
      }
    }
    std::sort(m.modes.begin(), m.modes.end(), std::greater<>());
    UINT dpi_x = 96;
    UINT dpi_y = 96;
    if (SUCCEEDED(GetDpiForMonitor(handle, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y))) {
      m.dpi = dpi_x;
    }
    if (auto it = names.find(info.szDevice); it != names.end()) {
      m.name = Utf8(it->second.c_str());
    }
    monitors.push_back(std::move(m));
  }
  std::stable_partition(monitors.begin(), monitors.end(), [](const Monitor& m) { return m.primary; });
  if (monitors.empty()) {
    Monitor m;
    m.primary = true;
    m.size = {GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
    m.work_right = m.size.first;
    m.work_bottom = m.size.second;
    monitors.push_back(m);
  }
  return monitors;
}

std::vector<Size> WindowSizes(const Monitor& m, Size keep) {
  RECT frame = {0, 0, 0, 0};
  AdjustWindowRectExForDpi(&frame, WS_OVERLAPPEDWINDOW, FALSE, 0, m.dpi);
  int max_width = (m.work_right - m.work_left) - (frame.right - frame.left);
  int max_height = (m.work_bottom - m.work_top) - (frame.bottom - frame.top);
  std::vector<Size> sizes;
  for (Size size : m.modes) {
    if (size.first >= kGuestWidth && size.second >= kGuestHeight && size.first <= max_width &&
        size.second <= max_height) {
      sizes.push_back(size);
    }
  }
  for (Size extra : {Size{kGuestWidth, kGuestHeight}, keep}) {
    if (extra.first > 0 && extra.second > 0 && std::find(sizes.begin(), sizes.end(), extra) == sizes.end()) {
      sizes.push_back(extra);
    }
  }
  std::sort(sizes.begin(), sizes.end(), std::greater<>());
  return sizes;
}

bool Is16x9(Size size) {
  return std::abs(int64_t(size.first) * 9 - int64_t(size.second) * 16) * 100 <= int64_t(size.second) * 16;
}

Size AutoScale(Size output) {
  double width = output.first;
  double height = output.second;
  if (width * 9 > height * 16) {
    width = height * 16 / 9;
  } else {
    height = width * 9 / 16;
  }
  auto axis = [](double size, int guest) { return std::clamp(int(std::ceil(size / guest - 0.1)), 1, kMaxScale); };
  return {axis(width, kGuestWidth), axis(height, kGuestHeight)};
}

// As the renderer computes it from its swap chain (native_x_renderer/src/renderer.cpp). The image fills an
// output that is not 16:9 (`fill`, native_x_renderer_fill_screen: the height sets the scale, the width follows
// from the output's aspect ratio), or is the 16:9 rectangle inside it. Its height is the output's (or the
// rectangle's), or `height` when one is chosen (native_x_renderer_resolution_scale = height / 720); at least
// the game's own 1280x720 on each axis.
Size NativeRenderSize(Size output, bool fill, int height) {
  double guest_aspect = double(kGuestWidth) / kGuestHeight;
  double output_aspect = output.second ? double(output.first) / output.second : guest_aspect;
  fill = fill && std::abs(output_aspect / guest_aspect - 1.0) > 0.01;
  double factor = fill ? output_aspect / guest_aspect : 1.0;
  double fit = fill ? double(output.second) / kGuestHeight
                    : std::min(double(output.first) / kGuestWidth, double(output.second) / kGuestHeight);
  double scale = std::max(height > 0 ? double(height) / kGuestHeight : fit, 1.0 / std::min(factor, 1.0));
  return {int(std::floor(kGuestWidth * scale * factor + 0.5)), int(std::floor(kGuestHeight * scale + 0.5))};
}

// 0 (automatic: the output's), then the usual heights below it, down to the lowest the image allows (720
// lines; more for an output narrower than 16:9). `keep` (a height the options file named) is offered too if it
// is in that range. Nothing above the output's: no supersampling.
std::vector<int> NativeHeights(Size output, bool fill, int keep) {
  int highest = NativeRenderSize(output, fill, 0).second;
  int lowest = NativeRenderSize(output, fill, kGuestHeight).second;
  std::vector<int> heights;
  for (int height : {2160, 1800, 1440, 1080, 900, 720, lowest, keep}) {
    if (height >= lowest && height < highest && std::find(heights.begin(), heights.end(), height) == heights.end()) {
      heights.push_back(height);
    }
  }
  std::sort(heights.begin(), heights.end(), std::greater<>());
  heights.insert(heights.begin(), 0);
  return heights;
}

// --- The settings being edited ---

const Monitor& Settings::CurrentMonitor() const {
  return monitors[std::clamp(o.monitor, 1, int(monitors.size())) - 1];
}

Size Settings::OutputSize() const { return o.display == kWindow ? o.window : CurrentMonitor().size; }

Size Settings::EffectiveScale() const { return o.scale == 0 ? AutoScale(OutputSize()) : Size{o.scale, o.scale}; }

void Settings::MonitorChanged() {
  window_sizes = WindowSizes(CurrentMonitor(), {0, 0});
  FitFrameRate();
  if (std::find(window_sizes.begin(), window_sizes.end(), o.window) == window_sizes.end()) {
    o.window = {kGuestWidth, kGuestHeight};
  }
}

void Settings::FitFrameRate() {
  std::vector<game::FrameRate> rates = FrameRates(CurrentMonitor());
  if (std::none_of(rates.begin(), rates.end(), [this](const game::FrameRate& r) { return r.value == o.frame_rate; })) {
    o.frame_rate = game::kFrameRateDefault;
  }
}

Settings LoadSettings(const std::filesystem::path& exe_dir) {
  Settings s;
  s.options_path = exe_dir / game::kOptionsFile;
  // The same the game will see: its base settings, then what the launcher saved on top.
  Config config;
  ReadToml(exe_dir / game::kConfigFile, config);
  ReadToml(s.options_path, config);
  s.monitors = EnumerateMonitors();
  s.primary_dpi = s.monitors.front().dpi;
  s.o = OptionsFromConfig(config, s.primary_dpi);
  // A monitor that is not connected now (a TV) stays chosen: the page shows the primary for it
  // (CurrentMonitor), and the file keeps it unless another one is picked.
  s.window_sizes = WindowSizes(s.CurrentMonitor(), s.o.window);
  if (HMODULE vulkan = LoadLibraryW(L"vulkan-1.dll")) {
    s.has_vulkan = true;
    FreeLibrary(vulkan);
  }
  s.o.vulkan = s.o.vulkan && s.has_vulkan;
  s.o.native = s.o.native && s.has_vulkan;
  s.FitFrameRate();
  return s;
}

bool SaveOptions(const Settings& s) {
  const Options& o = s.o;
  Size scale = s.EffectiveScale();
  std::ofstream f(s.options_path, std::ios::trunc);
  if (!f) {
    return false;
  }
  auto boolean = [](bool value) { return value ? "true" : "false"; };
  f << "# " << game::kLauncherName << " options (\"" << game::kLauncherExecutable << "\"): written whenever a\n"
       "# setting changes. " << game::kRecompExecutable << " loads them at startup, after " << game::kConfigFile
    << ", without\n"
       "# showing any window.\n"
       "# Command line options (for example those of a Steam shortcut) win over these.\n"
       "# monitor: 1 = primary, 2.. = the others (SDL order).\n"
       "# launcher_*: launcher only. launcher_render_scale is the choice (\"auto\" or 1-3); the\n"
       "# actual scale is draw_resolution_scale_x/y (with \"auto\", computed for the monitor or the\n"
       "# window). launcher_window_*: the chosen window size, kept even when playing fullscreen\n"
       "# (window_width/height are not written then: they are the console video mode).\n"
       "# launcher_ui_*: the launcher's own window.\n";
  f << "monitor = " << o.monitor << "\n";
  f << "fullscreen = " << boolean(o.display != kWindow) << "\n";
  f << "fullscreen_exclusive = " << boolean(o.display == kFullscreen) << "\n";
  // window_width/height are also the console video mode (VdQueryVideoMode, when there is no
  // video_mode_width/height), and the presenter takes the image aspect ratio from it.
  // In fullscreen they are not written: 1280x720 mode, as always. In a window they are (in logical
  // pixels: the SDK converts them with the primary monitor's scale), and if the window is not 16:9
  // a separate 16:9 mode is set so the image is letterboxed instead of distorted (1280x720 does not
  // work: it is the default value and the SDK would fall back to the window size).
  int window_width = MulDiv(o.window.first, 96, int(s.primary_dpi));
  int window_height = MulDiv(o.window.second, 96, int(s.primary_dpi));
  if (o.display == kWindow) {
    f << "window_width = " << window_width << "\n";
    f << "window_height = " << window_height << "\n";
    if (!Is16x9(o.window)) {
      f << "video_mode_width = 1920\n";
      f << "video_mode_height = 1080\n";
    }
  }
  f << "launcher_window_width = " << window_width << "\n";
  f << "launcher_window_height = " << window_height << "\n";
  if (o.scale == 0) {
    f << "launcher_render_scale = \"auto\"\n";
  } else {
    f << "launcher_render_scale = " << o.scale << "\n";
  }
  // resolution_scale = 1 so that draw_resolution_scale_x/y win (even if the base settings say
  // otherwise). With NativeXRenderer the SDK's backend only keeps the GPU in sync, at 1x;
  // launcher_render_scale keeps the choice for Xenos.
  if (o.native) {
    scale = {1, 1};
  }
  f << "resolution_scale = 1\n";
  f << "draw_resolution_scale_x = " << scale.first << "\n";
  f << "draw_resolution_scale_y = " << scale.second << "\n";
  f << "gpu_backend = \"" << (o.vulkan ? "vulkan" : "d3d12") << "\"\n";
  // The game's own options (game.h).
  if (*game::kFrameRateCvar) {
    // 60 FPS (src/framerate.cpp), or the emulated vblank's rate the game follows.
    if (game::kFrameRateIsBool) {
      f << game::kFrameRateCvar << " = " << boolean(o.frame_rate != 0) << "\n";
    } else {
      f << game::kFrameRateCvar << " = " << o.frame_rate << "\n";
    }
  }
  if (*game::kNoMsaaCvar) {
    // Render surfaces without the game's MSAA (src/msaa.cpp): less GPU work, no edge antialiasing.
    f << game::kNoMsaaCvar << " = " << boolean(o.no_msaa) << "\n";
  }
  if (*game::kUltrawideCvar) {
    // The image format for both renderers: the cameras widened (src/widescreen.cpp) and the Xenos path's image
    // stretched over a screen wider than 16:9, or 16:9 with bars.
    f << game::kUltrawideCvar << " = " << boolean(o.native_fill) << "\n";
    f << "present_letterbox = " << boolean(!o.native_fill) << "\n";
  }
  if (!game::kLanguages.empty()) {
    // The game's language, and its version where it has two (each variant cvar written, set for the one chosen).
    const game::Language& chosen = game::kLanguages[std::clamp(o.language, 0, int(game::kLanguages.size()) - 1)];
    f << "user_language = " << chosen.id << "\n";
    for (const game::Language& l : game::kLanguages) {
      if (l.variant_cvar) {
        f << l.variant_cvar << " = " << boolean(&l == &chosen) << "\n";
      }
    }
  }
  for (size_t i = 0; i < game::kToggles.size(); ++i) {
    f << game::kToggles[i].cvar << " = " << boolean(o.toggles[i]) << "\n";
  }
  // Anisotropic filtering for textures with mipmaps, sharper at oblique angles (SDK cvar; the game itself
  // asks for 4x on some textures only): 5 = 16x, 4 = 8x, 3 = 4x, 2 = 2x, 0 = off.
  f << "anisotropic_override = " << o.anisotropic << "\n";
  // NativeXRenderer (native_x_renderer/): draw natively on Vulkan instead of emulating the GPU (the default, D56).
  if (game::kHasNativeXRenderer) {
    f << "native_x_renderer = " << boolean(o.native) << "\n";
    // NativeXRenderer's image format and internal resolution (D54).
    f << "# native_x_renderer_fill_screen: NativeXRenderer fills a screen or window that is not 16:9 (true), or keeps\n"
         "# 16:9 with bars (false). native_x_renderer_resolution_scale: its internal resolution, as a multiple of\n"
         "# the game's frame height (2 = twice its lines); 0 = the screen's or the window's.\n";
    f << "native_x_renderer_fill_screen = " << boolean(o.native_fill) << "\n";
    {
      char text[32];
      std::snprintf(text, sizeof(text), "%.6f", o.native_height > 0 ? double(o.native_height) / kGuestHeight : 0.0);
      std::string value = text;
      value.erase(value.find_last_not_of('0') + 1);
      if (value.back() == '.') value.pop_back();
      f << "native_x_renderer_resolution_scale = " << value << "\n";
    }
    // NativeXRenderer's own antialiasing: smaa, dlaa, fxaa or off; or an experimental mode the file named (D53).
    f << "# native_x_renderer_antialiasing: smaa, dlaa, fxaa or off, the launcher's choices. Experimental, for\n"
         "# testing only (not finished, so the launcher does not offer them): dlss_quality, dlss_balanced,\n"
         "# dlss_performance and dlss_ultra_performance, NVIDIA DLSS upscaling the 3D view from a lower\n"
         "# resolution. The launcher keeps one written here.\n";
    const char* native_aa = o.native_aa < kNativeAntialiasingCount ? kNativeAntialiasingNames[std::max(o.native_aa, 0)]
                            : o.native_aa_experimental >= 0   ? kExperimentalAntialiasing[o.native_aa_experimental].name
                                                              : kNativeAntialiasingNames[0];
    f << "native_x_renderer_antialiasing = \"" << native_aa << "\"\n";
  }
  // The keyboard and the mouse as the player's controller (the SDK's mnk driver): its keys are lists of
  // alternatives ("Q,I"), each optionally with Shift+, Ctrl+ or Alt+.
  f << "# Keyboard and mouse: mnk_mode plays with them; mnk_mouse moves the camera (the right stick) with the\n"
       "# mouse, mnk_sensitivity its speed; keybind_*: each control's keys, comma-separated alternatives.\n";
  f << "mnk_mode = " << boolean(o.mnk) << "\n";
  f << "mnk_mouse = " << boolean(o.mnk_mouse) << "\n";
  {
    char text[32];
    std::snprintf(text, sizeof(text), "%.2f", double(o.mnk_sensitivity));
    f << "mnk_sensitivity = " << text << "\n";
  }
  for (int i = 0; i < int(Bind::kCount); ++i) {
    f << kBinds[i].cvar << " = \"" << o.binds[i] << "\"\n";
  }
  // The controllers' layouts: per SDL gamepad type, which button presses each Xbox 360 control
  // ("ps5:a=east,b=south"); empty: the standard layout.
  f << "gamepad_remap = \"" << o.gamepad_remap << "\"\n";
  f << "launcher_check_updates = " << boolean(o.check_updates) << "\n";
  if (o.last_update_check > 0) {
    f << "launcher_last_update_check = " << o.last_update_check << "\n";
  }
  f << "launcher_ui_fullscreen = " << boolean(o.ui_fullscreen) << "\n";
  f << "launcher_ui_maximized = " << boolean(o.ui_maximized) << "\n";
  if (o.ui_size.first > 0 && o.ui_size.second > 0) {
    f << "launcher_ui_width = " << o.ui_size.first << "\n";
    f << "launcher_ui_height = " << o.ui_size.second << "\n";
  }
  return bool(f);
}

}  // namespace launcher
