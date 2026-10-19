// kameo - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <rex/rex_app.h>

#include <rex/filesystem.h>
#include <rex/kernel/xam/module.h>
#include <rex/logging.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xthread.h>

#include <native_x_renderer/native_x_renderer.h>

#include "native_x_renderer_kameo.h"
#include "options.h"
#include "startup.h"
#include "widescreen.h"

class KameoApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<KameoApp>(new KameoApp(ctx, "kameo",
        PPCImageConfig));
  }

  // Override virtual hooks for customization:
  // void OnPostInitLogging() override {}
  // Use the "xenos" GPU emulation (rexgpu-xenos.dll) unless another one was given.
  void OnPreSetup(rex::RuntimeConfig& config) override {
    if (config.gpu_plugin.empty()) {
      config.gpu_plugin = "xenos";
    }
  }
  // void OnLoadXexImage(std::string& xex_image) override {}
  // void OnPostLoadXexImage() override {}
  // void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {}
  // std::unique_ptr<rex::ui::ImGuiDialog> CreateAchievementsOverlay() override;
  // std::unique_ptr<rex::ui::AchievementNotificationDialog>
  // CreateAchievementNotificationDialog() override;
  // void OnShutdown() override {}
  // Game files: without --game_data_root, assets/ next to kameorecomp.exe or the repository's
  // assets/. User data (saves, profile, shader cache): userdata/ next to the executable
  // (src/startup.cpp).
  void OnConfigurePaths(rex::PathConfig& paths) override {
    if (paths.game_data_root.empty()) {
      paths.game_data_root = kameo::DefaultGameRoot();
    }
    kameo::ConfigureUserData(paths.user_data_root, paths.cache_root);
  }

  // kameo.toml with the built-in defaults if it is missing, then (after the SDK loads it) the game
  // files check and the Kameo Launcher options (separate executable, launcher/kameo_launcher.cpp):
  // kameo_options.toml, before the window and the GPU are created. The game shows no window of
  // its own.
  bool SetupEnvironment() override {
    auto exe_dir = rex::filesystem::GetExecutableFolder();
    kameo::EnsureDefaultConfig(exe_dir / "kameo.toml");
    if (!rex::ReXApp::SetupEnvironment()) return false;
    REXLOG_INFO("[startup] User data: {}", user_data_root().string());
    REXLOG_INFO("[startup] {}", native_x_renderer::Describe());
    if (!kameo::CheckGameFiles(game_data_root())) return false;
    kameo::LoadLauncherOptions(exe_dir / "kameo_options.toml", kameo::GpuPluginPath());
    return true;
  }

  // Window title (the SDK sets "<app name> <SDK build stamp>"), then NativeXRenderer, before the guest starts: its
  // hooks (src/native_x_renderer_kameo.cpp) are pass-throughs until it is initialized; then ultrawide
  // (src/widescreen.cpp): the cameras' aspect ratio follows the window, or NativeXRenderer's when it draws.
  void OnPostSetup() override {
    if (window()) {
      window()->SetTitle("Kameo: Elements of Power");
    }
    native_x_renderer::Initialize(kameo::NativeXRendererProfile(), runtime() ? runtime()->graphics_system() : nullptr,
                                  window(), runtime() ? runtime()->cache_root() : std::filesystem::path());
    if (window()) {
      kameo::SetOutputSize(window()->GetActualPhysicalWidth(), window()->GetActualPhysicalHeight());
    }
  }
  void OnWindowPixelSizeChanged(uint32_t pixel_width, uint32_t pixel_height) override {
    kameo::SetOutputSize(pixel_width, pixel_height);
  }

  // On exit, log whether the game asked to launch another executable.
  void OnGuestThreadExit(rex::system::XThread* thread) override {
    (void)thread;
    auto* rt = runtime();
    if (!rt || !rt->kernel_state()) return;
    auto xam = rt->kernel_state()->GetKernelModule<rex::kernel::xam::XamModule>("xam.xex");
    if (!xam) return;
    auto& ld = xam->loader_data();
    REXLOG_INFO("[kameo] Title exited. launch_path='{}' flags=0x{:X} launch_data={} bytes",
                ld.launch_path, ld.launch_flags, ld.launch_data.size());
  }
};
