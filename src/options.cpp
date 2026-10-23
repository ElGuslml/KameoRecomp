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

  // The frame rate (src/framerate.cpp): Kameo Launcher writes kameo_frame_rate. Above 30 the game presents every
  // vblank (kameo_60fps); above 60 the vblank itself runs at that rate (the SDK's video_mode_refresh_rate, read when
  // the graphics system starts, after this). Without it (0, an older options file) kameo_60fps decides as before.
  if (int32_t frame_rate = rex::cvar::Query<int32_t>("kameo_frame_rate"); frame_rate > 0) {
    rex::cvar::SetFlagByName("kameo_60fps", frame_rate > 30 ? "true" : "false");
    rex::cvar::SetFlagByName("video_mode_refresh_rate", std::to_string(frame_rate > 60 ? frame_rate : 60));
  }

  // NativeXRenderer draws through its own Vulkan device (DiligentCore), and the SDK's backend only keeps the
  // GPU in sync then. Its Vulkan backend keeps the process on one graphics API: no idle D3D12 device next
  // to NativeXRenderer's, and overlays such as RTSS report the API that presents (as POTCAWE, D27).
  if (rex::cvar::Query<bool>("native_x_renderer") && rex::cvar::Query<std::string>("gpu_backend") != "vulkan") {
    rex::cvar::SetFlagByName("gpu_backend", "vulkan");
    REXLOG_INFO("[options] gpu_backend set to vulkan: NativeXRenderer draws");
  }

  // Ultrawide on the Xenos path (src/widescreen.cpp): the cameras see with the window's aspect ratio, so the
  // presenter must stretch the 1280x720 image over the window instead of showing it 16:9 with bars. Kameo
  // Launcher writes both; this covers kameo_ultrawide given alone (the command line). NativeXRenderer fills the
  // window itself (native_x_renderer_fill_screen).
  if (rex::cvar::Query<bool>("kameo_ultrawide") && !rex::cvar::Query<bool>("native_x_renderer") &&
      rex::cvar::Query<bool>("present_letterbox")) {
    rex::cvar::SetFlagByName("present_letterbox", "false");
    REXLOG_INFO("[options] present_letterbox set to false: kameo_ultrawide stretches the image");
  }

  bool fullscreen = rex::cvar::Query<bool>("fullscreen");
  REXLOG_INFO("[options] monitor={} scale={}x{} display={} window={}x{} video_mode={}x{} "
              "letterbox={} ultrawide={} 60fps={} refresh={} gpu_backend={} anisotropic={} native_x_renderer={} "
              "(resolution_scale={} fill_screen={}) "
              "language={}{}",
              rex::cvar::Query<int32_t>("monitor"),
              rex::cvar::Query<int32_t>("draw_resolution_scale_x"),
              rex::cvar::Query<int32_t>("draw_resolution_scale_y"),
              fullscreen ? (rex::cvar::Query<bool>("fullscreen_exclusive") ? "fullscreen" : "borderless")
                         : "window",
              rex::cvar::Query<int32_t>("window_width"), rex::cvar::Query<int32_t>("window_height"),
              rex::cvar::Query<int32_t>("video_mode_width"),
              rex::cvar::Query<int32_t>("video_mode_height"),
              rex::cvar::Query<bool>("present_letterbox"), rex::cvar::Query<bool>("kameo_ultrawide"),
              rex::cvar::Query<bool>("kameo_60fps"), rex::cvar::Query<double>("video_mode_refresh_rate"),
              rex::cvar::Query<std::string>("gpu_backend"), rex::cvar::Query<int32_t>("anisotropic_override"),
              rex::cvar::Query<bool>("native_x_renderer"),
              rex::cvar::Query<double>("native_x_renderer_resolution_scale"),
              rex::cvar::Query<bool>("native_x_renderer_fill_screen"),
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
