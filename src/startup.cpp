// kameo - startup (see startup.h).

#include "startup.h"
#include "profile_store.h"

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/logging.h>
#include <rex/system/flags.h>

#include <array>
#include <cstdint>
#include <fstream>
#include <optional>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace kameo {
namespace {

constexpr const char* kGameExecutable = "default.xex";
// CRC32 of the default.xex this code was recompiled from (3,682,304 bytes, title ID 4D5307D2,
// version 0.0.0.1).
constexpr uint32_t kGameExecutableCrc32 = 0xE24BE720;

// The GPU settings are POTCAWERecomp's: they are not yet shown to be needed by this game. Each
// comment says what the setting does and what false (or true) gives back.
constexpr const char kDefaultConfig[] =
    R"(# KameoEOPRecomp.exe runtime configuration, loaded at startup. The game recreates this file with
# these defaults if it is missing. Kameo Launcher saves its choices to kameo_options.toml, which
# is loaded after this file; command line options (--name=value) win over both.
log_level = "info"
# Do not wait at the end of the frame for new D3D12 pipelines (a stutter the first time an effect
# appears): the object appears a few frames later (SDK patch 0009). false = wait.
gpu_async_pipelines_no_frame_wait = true
# Translate the shaders' control flow with if blocks instead of a switch. In POTCAWERecomp the
# switch (the SDK's default) made the pixel shader body re-execute under GPU load and lit objects
# turned into flat saturated colors. true = the SDK default.
dxbc_switch = false
# Clears drawn through a surface with another MSAA mode or pitch than the scene: without these
# settings the SDK copies the render target into that alias and back around each clear. Run the
# vertex shader of unclipped draws on the CPU to know how much EDRAM they use (the Xenia default;
# the SDK default is false)...
execute_unclipped_draw_vs_on_cpu = true
# ...and skip the copy into a render target when the draw replaces every sample of it (SDK patch
# 0017).
render_target_skip_transfers_on_overwrite = true
# Write constant full-coverage draws (clears) straight into the render targets that hold that
# memory (SDK patch 0018). Off: with it the first cinematic of Thorn's Castle shows only the sky
# and the glows, no geometry.
render_target_draw_clears = false
# Keep the GPU copies of memory the CPU uploaded valid across frames. With the SDK default (true)
# every page uploaded by the CPU is re-uploaded the next frame even if it did not change. false is
# the original Xenia behavior (true is a hack for one game).
clear_memory_page_state = false
# Anisotropic filtering for textures with mipmaps, sharper at oblique angles (both renderers):
# 5 = 16x, 4 = 8x, 3 = 4x (the SDK default), 2 = 2x, 0 = off, -1 = only what the game asks. Kameo
# Launcher's "Anisotropic filtering" row sets it.
anisotropic_override = 5
# Draw the game with NativeXRenderer, a native renderer on Vulkan (DiligentCore), instead of the SDK's emulation
# of the Xbox 360 GPU (docs/native-x-renderer-kameo.md). Kameo Launcher's "Renderer" row sets it. false = the
# Xenos path. If NativeXRenderer cannot start, the Xenos path draws.
native_x_renderer = true
# NativeXRenderer (native_x_renderer = true, Kameo Launcher's "Renderer" row) renders at the aspect ratio of
# a screen that is not 16:9 only with this on; Kameo's ultrawide hooks widen the cameras for a wider screen,
# not a taller one. Kameo Launcher turns it on with "Ultrawide" on a wider screen. false = 16:9 with bars.
native_x_renderer_fill_screen = false
# NativeXRenderer shows each frame when the game's swap for it executes, as the Xenos path does (library
# D86). The game's Bink code waits a vertical blank for the GPU now and then while it plays a video; shown
# at the game's Present calls instead (false), those frames took 30-50 ms.
native_x_renderer_present_with_swaps = true
)";

uint32_t Crc32(const std::vector<char>& data) {
  static const std::array<uint32_t, 256> table = [] {
    std::array<uint32_t, 256> t{};
    for (uint32_t i = 0; i < 256; ++i) {
      uint32_t c = i;
      for (int k = 0; k < 8; ++k) {
        c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
      }
      t[i] = c;
    }
    return t;
  }();
  uint32_t crc = 0xFFFFFFFFu;
  for (char byte : data) {
    crc = table[(crc ^ uint8_t(byte)) & 0xFF] ^ (crc >> 8);
  }
  return crc ^ 0xFFFFFFFFu;
}

#ifdef _WIN32
// MB_* flags; returns the button pressed.
int ShowMessage(const std::wstring& text, UINT flags) {
  return MessageBoxW(nullptr, text.c_str(), L"Kameo: Elements of Power", flags);
}

std::wstring Hex32(uint32_t value) {
  wchar_t buffer[16];
  swprintf(buffer, 16, L"%08X", value);
  return buffer;
}
#endif

}  // namespace

std::filesystem::path DefaultGameRoot() {
  std::filesystem::path next_to_exe = rex::filesystem::GetExecutableFolder() / "assets";
  if (std::filesystem::exists(next_to_exe / kGameExecutable)) {
    return next_to_exe;
  }
  std::filesystem::path repository = KAMEO_DEFAULT_GAME_ROOT;
  if (std::filesystem::exists(repository / kGameExecutable)) {
    return repository;
  }
  // Neither has the game: report the folder next to the executable (the release layout), not a path
  // of the machine the build was made on.
  return next_to_exe;
}

void ConfigureUserData(std::filesystem::path& user_data_root, std::filesystem::path& cache_root) {
  if (!rex::cvar::Query<std::string>("user_data_root").empty()) {
    return;  // chosen on the command line
  }
  std::filesystem::path portable = rex::filesystem::GetExecutableFolder() / "userdata";
  std::error_code ec;
  std::filesystem::create_directories(portable, ec);
  // Writable? Logging is not set up yet: the folder in use is logged later
  // (KameoApp::SetupEnvironment).
  std::filesystem::path probe = portable / ".write_test";
  {
    std::ofstream f(probe, std::ios::binary);
    if (!f) {
      return;
    }
  }
  std::filesystem::remove(probe, ec);
  user_data_root = portable;
  if (rex::cvar::Query<std::string>("cache_root").empty()) {
    cache_root = portable / "cache";
  }
}

void ConfigureProfile(const std::filesystem::path& user_data_root) {
  if (!REXCVAR_GET(profile_root).empty()) {
    return;  // chosen on the command line
  }
  if (std::optional<launcher::profiles::GameProfile> profile = launcher::profiles::ForGame(user_data_root)) {
    std::u8string folder = profile->folder.u8string();
    REXCVAR_SET(profile_root, std::string(folder.begin(), folder.end()));
    if (REXCVAR_GET(profile_name).empty()) {
      REXCVAR_SET(profile_name, profile->name);
    }
  }
}

// Creates kameo.toml with the defaults, or appends to an existing one the settings (with their
// comments) it does not mention, so that a setting added in a newer build takes effect without
// deleting the user's file. A setting the file has, with any value, is left alone.
void EnsureDefaultConfig(const std::filesystem::path& config_path) {
  // Logging is not initialized yet (the config sets it up): nothing to log here.
  std::string existing;
  if (std::filesystem::exists(config_path)) {
    std::ifstream in(config_path, std::ios::binary);
    existing.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  } else {
    std::ofstream f(config_path, std::ios::binary);
    f << kDefaultConfig;
    return;
  }
  auto has_key = [&existing](const std::string& key) {
    size_t pos = 0;
    while ((pos = existing.find(key, pos)) != std::string::npos) {
      size_t line_start = existing.rfind('\n', pos);
      line_start = line_start == std::string::npos ? 0 : line_start + 1;
      size_t after = pos + key.size();
      bool only_spaces_before = existing.find_first_not_of(" \t", line_start) == pos;
      size_t next = existing.find_first_not_of(" \t", after);
      if (only_spaces_before && next != std::string::npos && existing[next] == '=') {
        return true;
      }
      pos = after;
    }
    return false;
  };
  std::string missing;
  std::string comments;
  std::istringstream defaults(kDefaultConfig);
  std::string line;
  while (std::getline(defaults, line)) {
    if (line.empty()) {
      comments.clear();
      continue;
    }
    if (line[0] == '#') {
      comments += line + '\n';
      continue;
    }
    size_t eq = line.find('=');
    if (eq == std::string::npos) {
      comments.clear();
      continue;
    }
    std::string key = line.substr(0, eq);
    key.erase(key.find_last_not_of(" \t") + 1);
    if (!has_key(key)) {
      missing += comments + line + '\n';
    }
    comments.clear();
  }
  if (missing.empty()) {
    return;
  }
  std::ofstream f(config_path, std::ios::binary | std::ios::app);
  if (!existing.empty() && existing.back() != '\n') {
    f << '\n';
  }
  f << missing;
}

bool CheckGameFiles(const std::filesystem::path& game_root) {
  std::filesystem::path exe = game_root / kGameExecutable;
  std::ifstream f(exe, std::ios::binary);
  if (!f) {
    REXLOG_ERROR("[startup] Game files not found: {} does not exist", exe.string());
#ifdef _WIN32
    ShowMessage(L"Game files not found.\n\n"
                L"Copy the files of your Kameo: Elements of Power (Xbox 360) disc "
                L"into:\n" +
                    game_root.wstring() + L"\n\nso that this file exists:\n" + exe.wstring(),
                MB_OK | MB_ICONERROR);
#endif
    return false;
  }
  std::vector<char> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  uint32_t crc = Crc32(data);
  if (crc == kGameExecutableCrc32) {
    REXLOG_INFO("[startup] {}: CRC32 {:08X} (expected version)", exe.string(), crc);
    return true;
  }
  REXLOG_WARN("[startup] {}: CRC32 {:08X}, expected {:08X}: a different version of the game",
              exe.string(), crc, kGameExecutableCrc32);
#ifdef _WIN32
  int answer = ShowMessage(L"default.xex is not the version this build was recompiled from "
                           L"(CRC32 " + Hex32(crc) + L", expected " + Hex32(kGameExecutableCrc32) +
                               L").\nThe game will most likely crash.\n\nStart anyway?",
                           MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
  return answer == IDYES;
#else
  return true;
#endif
}

}  // namespace kameo
