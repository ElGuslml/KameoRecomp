// kameo - Kameo Launcher options (see options.h).

#include "options.h"

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/logging.h>

#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace kameo {

void LoadLauncherOptions(const std::filesystem::path& options_path,
                         const std::filesystem::path& gpu_plugin_path) {
#ifdef _WIN32
  // The render scale cvars (draw_resolution_scale_x/y, resolution_scale) live in the GPU DLL,
  // which the SDK loads later (SetupPresentation). Loading it now registers them, so the log line
  // below shows the effective values (otherwise LoadConfig keeps them pending and applies them
  // when they register). It stays loaded: LoadGpuPlugin opens the same DLL later.
  if (!LoadLibraryW(gpu_plugin_path.c_str())) {
    REXLOG_WARN("[options] Could not load {} ({})", gpu_plugin_path.string(), GetLastError());
  }
#endif

  // Loaded after kameo.toml, so it takes precedence; the command line (for example Steam launch
  // options) wins over both.
  if (std::filesystem::exists(options_path)) {
    rex::cvar::LoadConfig(options_path);
  } else {
    REXLOG_INFO("[options] {} does not exist: using kameo.toml (Kameo Launcher creates it)",
                options_path.string());
  }

  // NativeXRenderer draws through its own Vulkan device (DiligentCore), and the SDK's backend only keeps the
  // GPU in sync then. Its Vulkan backend keeps the process on one graphics API: no idle D3D12 device next
  // to NativeXRenderer's, and overlays such as RTSS report the API that presents (as POTCAWE, D27).
  if (rex::cvar::Query<bool>("native_x_renderer") && rex::cvar::Query<std::string>("gpu_backend") != "vulkan") {
    rex::cvar::SetFlagByName("gpu_backend", "vulkan");
    REXLOG_INFO("[options] gpu_backend set to vulkan: NativeXRenderer draws");
  }

  bool fullscreen = rex::cvar::Query<bool>("fullscreen");
  REXLOG_INFO("[options] monitor={} scale={}x{} display={} window={}x{} video_mode={}x{} "
              "letterbox={} 60fps={} gpu_backend={} anisotropic={} native_x_renderer={} (resolution_scale={}) "
              "language={}{}",
              rex::cvar::Query<int32_t>("monitor"),
              rex::cvar::Query<int32_t>("draw_resolution_scale_x"),
              rex::cvar::Query<int32_t>("draw_resolution_scale_y"),
              fullscreen ? (rex::cvar::Query<bool>("fullscreen_borderless_window") ? "borderless"
                                                                                  : "fullscreen")
                         : "window",
              rex::cvar::Query<int32_t>("window_width"), rex::cvar::Query<int32_t>("window_height"),
              rex::cvar::Query<int32_t>("video_mode_width"),
              rex::cvar::Query<int32_t>("video_mode_height"),
              rex::cvar::Query<bool>("present_letterbox"), rex::cvar::Query<bool>("kameo_60fps"),
              rex::cvar::Query<std::string>("gpu_backend"), rex::cvar::Query<int32_t>("anisotropic_override"),
              rex::cvar::Query<bool>("native_x_renderer"),
              rex::cvar::Query<double>("native_x_renderer_resolution_scale"),
              rex::cvar::Query<uint32_t>("user_language"),
              rex::cvar::Query<bool>("kameo_latin_american_spanish") ? " (Latin American)" : "");
}

std::filesystem::path GpuPluginPath() {
  std::filesystem::path exe_dir = rex::filesystem::GetExecutableFolder();
#if defined(_WIN32)
  const char* names[] = {"rexgpu-xenos.dll", "rexgpu-xenosrd.dll", "rexgpu-xenosd.dll"};
#elif defined(__APPLE__)
  const char* names[] = {"librexgpu-xenos.dylib", "librexgpu-xenosrd.dylib",
                         "librexgpu-xenosd.dylib"};
#else
  const char* names[] = {"librexgpu-xenos.so", "librexgpu-xenosrd.so", "librexgpu-xenosd.so"};
#endif
  for (const char* name : names) {
    if (std::filesystem::exists(exe_dir / name)) {
      return exe_dir / name;
    }
  }
  return exe_dir / names[0];
}

}  // namespace kameo
