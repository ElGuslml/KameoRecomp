// The game's launcher (game.h names it and the game): options, the first run from the player's ISO,
// achievements and updates, in a separate executable, so that the game's own starts right away (for example
// from Steam Big Picture).
//
// Launcher 2 (made for POTCAWERecomp, shared through recomp-tools' template): Gustavo's mockup on SDL3 and
// Dear ImGui, drawn with Direct3D 11, all from the SDK tree and linked statically, so it is one executable without the SDK's DLLs.
// It reads the game's base settings and the options file (game::kConfigFile, game::kOptionsFile, next to this
// exe and the game's), saves every change to the options file at once (settings.cpp), which the game loads at
// startup (src/options.cpp), and works with the mouse, the keyboard and any controller SDL knows (the
// DualSense included).
//
// The window scales with its size (the mockup's 1280x760 at 100 %) and fills the screen by default (always on the
// Steam Deck and in Big Picture); F11 or Alt+Enter switch it. The texts follow the Windows display language
// (English, German, French, Spanish, Italian); the game asks for its own language in its first menu.
//
// "Play" starts the game (with the arguments the launcher received) and the launcher stays:
// it shows "Starting the game…" until the game's window appears, hides, and comes back on the Play page when the
// game exits (with a notice if it closed with an error). So the launcher and the game look like one program, and
// Steam sees it running from the launcher's start to its end. The game runs in a job object that is closed with
// the launcher: if Steam stops the launcher, the game stops too. The launcher lets go of the controllers while the
// game runs.

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellscalingapi.h>

#include <SDL3/SDL.h>
#define SDL_MAIN_HANDLED  // wWinMain is this file's; SDL is told with SDL_SetMainReady()
#include <SDL3/SDL_main.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_sdl3.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cwchar>
#include <filesystem>
#include <initializer_list>
#include <string>

#include "app.h"
#include "game.h"
#include "ui/gfx.h"
#include "ui/ui.h"

namespace {

using launcher::App;

// The mockup's window and the narrowest layout the pages are made for, in dp: the scale follows the
// window's height, unless the window is narrower than that layout.
constexpr float kDesignWidth = 1280.0f;
constexpr float kDesignHeight = 760.0f;
constexpr float kLayoutWidth = 1200.0f;
// The smallest window, in dp.
constexpr float kMinWidth = 1000.0f;
constexpr float kMinHeight = 600.0f;

float ScaleFor(int width, int height) {
  float scale = std::min(float(height) / kDesignHeight, float(width) / kLayoutWidth);
  // In steps of 1/40: fonts and icons are drawn again only when it changes.
  return std::clamp(std::round(scale * 40.0f) / 40.0f, 0.5f, 4.0f);
}

bool EnvironmentIs(const char* name, const char* value) {
  char text[64];
  DWORD length = GetEnvironmentVariableA(name, text, sizeof(text));
  return length > 0 && length < sizeof(text) && std::string(text, length) == value;
}

// Steam sets SteamDeck=1 on the Steam Deck and SteamGamepadUI=1 in Big Picture (Proton passes both to the
// Windows environment): the launcher fills the screen there.
bool SteamWantsFullscreen() { return EnvironmentIs("SteamDeck", "1") || EnvironmentIs("SteamGamepadUI", "1"); }

std::filesystem::path ExecutableFolder() {
  std::wstring path(MAX_PATH, L'\0');
  for (;;) {
    DWORD length = GetModuleFileNameW(nullptr, path.data(), DWORD(path.size()));
    if (length < path.size()) {
      path.resize(length);
      break;
    }
    path.resize(path.size() * 2);
  }
  return std::filesystem::path(path).parent_path();
}

// What follows the program name on the command line, verbatim (with the leading space).
std::wstring ForwardedArguments() {
  const wchar_t* p = GetCommandLineW();
  if (*p == L'"') {
    ++p;
    while (*p && *p != L'"') {
      ++p;
    }
    if (*p) {
      ++p;
    }
  } else {
    while (*p && *p != L' ' && *p != L'\t') {
      ++p;
    }
  }
  return p;
}

// The arguments the launcher received, each with its leading space, without those starting with one of `drop`
// (a quoted argument stays whole).
std::wstring ArgumentsWithout(std::initializer_list<const wchar_t*> drop) {
  std::wstring all = ForwardedArguments();
  std::wstring kept;
  size_t at = 0;
  while (at < all.size()) {
    while (at < all.size() && (all[at] == L' ' || all[at] == L'\t')) ++at;
    if (at >= all.size()) break;
    size_t end = at;
    bool quoted = false;
    while (end < all.size() && (quoted || (all[end] != L' ' && all[end] != L'\t'))) {
      if (all[end] == L'"') quoted = !quoted;
      ++end;
    }
    std::wstring argument = all.substr(at, end - at);
    bool dropped = false;
    for (const wchar_t* prefix : drop) {
      dropped = dropped || argument.rfind(prefix, 0) == 0;
    }
    if (!dropped) kept += L" " + argument;
    at = end;
  }
  return kept;
}

// A text with its %s filled.
std::string Formatted(const char* format, const char* a, const char* b = "") {
  char text[1024];
  std::snprintf(text, sizeof(text), format, a, b);
  return text;
}

void ShowError(const char* text) {
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, launcher::game::kLauncherName, text, nullptr);
}

// A program the launcher starts and stays for: the game from Play, or the launcher's new version after an update.
class GameProcess {
 public:
  ~GameProcess() { Close(); }

  // `arguments` with their leading spaces. False with a message shown when it could not start.
  bool Start(const std::filesystem::path& exe, const std::wstring& arguments) {
    Close();
    std::wstring command_line = L"\"" + exe.wstring() + L"\"" + arguments;
    // Killed with the launcher: when Steam stops the program it started (this one), the game goes too. Nested
    // in Steam's own job, if any (Windows 8 and later).
    job_ = CreateJobObjectW(nullptr, nullptr);
    if (job_) {
      JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
      limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
      SetInformationJobObject(job_, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
    }
    STARTUPINFOW startup = {sizeof(startup)};
    PROCESS_INFORMATION process = {};
    AllowSetForegroundWindow(ASFW_ANY);
    std::filesystem::path folder = exe.parent_path();
    if (!CreateProcessW(exe.c_str(), command_line.data(), nullptr, nullptr, FALSE, CREATE_SUSPENDED, nullptr,
                        folder.c_str(), &startup, &process)) {
      std::string text = "CreateProcess: " + std::to_string(GetLastError());
      ShowError(text.c_str());
      Close();
      return false;
    }
    if (job_ && !AssignProcessToJobObject(job_, process.hProcess)) {
      CloseHandle(job_);  // the game still runs, only not tied to the launcher
      job_ = nullptr;
    }
    ResumeThread(process.hThread);
    CloseHandle(process.hThread);
    process_ = process.hProcess;
    id_ = process.dwProcessId;
    return true;
  }

  HANDLE handle() const { return process_; }
  bool Exited() const { return !process_ || WaitForSingleObject(process_, 0) == WAIT_OBJECT_0; }
  uint32_t ExitCode() const {
    DWORD code = 0;
    return process_ && GetExitCodeProcess(process_, &code) ? uint32_t(code) : 0;
  }

  // A window of the game is on screen: its own, or a message box it shows before it (a different version of
  // default.xex).
  bool WindowShown() const {
    struct Search {
      DWORD id;
      bool found;
    } search{id_, false};
    EnumWindows(
        [](HWND hwnd, LPARAM data) -> BOOL {
          auto* s = reinterpret_cast<Search*>(data);
          DWORD id = 0;
          GetWindowThreadProcessId(hwnd, &id);
          if (id != s->id || !IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER) ||
              (GetWindowLongW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW)) {
            return TRUE;
          }
          s->found = true;
          return FALSE;
        },
        LPARAM(&search));
    return search.found;
  }

  void Close() {
    if (process_) CloseHandle(process_);
    if (job_) CloseHandle(job_);
    process_ = nullptr;
    job_ = nullptr;
    id_ = 0;
  }

 private:
  HANDLE process_ = nullptr;
  HANDLE job_ = nullptr;
  DWORD id_ = 0;
};

// The launcher's window in front again after the game: Windows lets only the foreground window's thread give the
// foreground away, so this one joins its input for the moment (the game's window is gone; Big Picture or the
// desktop has it).
void BringToFront(HWND hwnd) {
  HWND foreground = GetForegroundWindow();
  DWORD foreground_thread = foreground ? GetWindowThreadProcessId(foreground, nullptr) : 0;
  DWORD this_thread = GetCurrentThreadId();
  bool attached = foreground_thread && foreground_thread != this_thread &&
                  AttachThreadInput(foreground_thread, this_thread, TRUE);
  BringWindowToTop(hwnd);
  SetForegroundWindow(hwnd);
  if (attached) {
    AttachThreadInput(foreground_thread, this_thread, FALSE);
  }
}

}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  // Physical pixels everywhere (monitors, modes, window) and sharp with display scaling.
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

  std::filesystem::path exe_dir = ExecutableFolder();
  launcher::Settings settings = launcher::LoadSettings(exe_dir);
  uint32_t language = launcher::SystemLanguage();
  const launcher::Texts& t = launcher::TextsFor(language);

  SDL_SetMainReady();
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
    ShowError(SDL_GetError());
    return 1;
  }

  // On the monitor of the active window (Big Picture, Explorer...), centered, at the size it had (or the
  // mockup's) times the monitor's scale, and no larger than its work area.
  HMONITOR monitor = MonitorFromWindow(GetForegroundWindow(), MONITOR_DEFAULTTOPRIMARY);
  MONITORINFO monitor_info = {sizeof(monitor_info)};
  GetMonitorInfoW(monitor, &monitor_info);
  const RECT& work = monitor_info.rcWork;
  int work_width = work.right - work.left;
  int work_height = work.bottom - work.top;
  float dpi_scale = float(GetDpiForSystem()) / 96.0f;
  if (UINT dpi = 0; SUCCEEDED(GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpi, &dpi))) {
    dpi_scale = float(dpi) / 96.0f;
  }
  launcher::Size logical = settings.o.ui_size.first > 0 && settings.o.ui_size.second > 0
                               ? settings.o.ui_size
                               : launcher::Size{int(kDesignWidth), int(kDesignHeight)};
  float width = float(logical.first) * dpi_scale;
  float height = float(logical.second) * dpi_scale;
  float fit = std::min({1.0f, float(work_width) * 0.92f / width, float(work_height) * 0.92f / height});
  int window_width = int(width * fit);
  int window_height = int(height * fit);
  int min_width = std::min(int(kMinWidth * dpi_scale), work_width);
  int min_height = std::min(int(kMinHeight * dpi_scale), work_height);
  window_width = std::max(window_width, min_width);
  window_height = std::max(window_height, min_height);

  SDL_Window* window = SDL_CreateWindow(launcher::game::kLauncherName, window_width, window_height,
                                        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (!window) {
    ShowError(SDL_GetError());
    SDL_Quit();
    return 1;
  }
  SDL_SetWindowPosition(window, work.left + (work_width - window_width) / 2, work.top + (work_height - window_height) / 2);
  SDL_SetWindowMinimumSize(window, min_width, min_height);
  bool fullscreen = settings.o.ui_fullscreen || SteamWantsFullscreen() ||
                    work_width < int(kMinWidth * dpi_scale) || work_height < int(kMinHeight * dpi_scale);
  if (settings.o.ui_maximized) {
    SDL_MaximizeWindow(window);
  }
  if (fullscreen) {
    SDL_SetWindowFullscreen(window, true);
  }

  HWND hwnd = HWND(SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
  if (!ui::gfx::Init(hwnd)) {
    ShowError(Formatted(t.no_direct3d, launcher::game::kLauncherName).c_str());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.LogFilename = nullptr;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  ImGui::GetStyle().FontSizeBase = 16.0f;
  // The widgets draw their own focus ring (ui::FocusRing).
  ImGui::GetStyle().Colors[ImGuiCol_NavCursor] = ImVec4(0, 0, 0, 0);
  ImGui_ImplSDL3_InitForD3D(window);
  ImGui_ImplDX11_Init(ui::gfx::Device(), ui::gfx::Context());
  if (!ui::LoadFonts()) {
    ShowError((std::string(launcher::game::kLauncherName) + ": the fonts are missing from the executable.").c_str());
    return 1;
  }

  App app(std::move(settings), language);
  app.SetWindow(window);
  // The window's size for the next time, in pixels at 100 % scaling, and every setting: before the game starts
  // and when the launcher closes.
  auto save = [&] {
    SDL_WindowFlags flags = SDL_GetWindowFlags(window);
    launcher::Options& o = app.settings().o;
    o.ui_maximized = (flags & SDL_WINDOW_MAXIMIZED) != 0;
    if (!(flags & (SDL_WINDOW_MAXIMIZED | SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN))) {
      int w = 0;
      int h = 0;
      SDL_GetWindowSizeInPixels(window, &w, &h);
      o.ui_size = {int(float(w) / dpi_scale + 0.5f), int(float(h) / dpi_scale + 0.5f)};
    }
    return launcher::SaveOptions(app.settings());
  };
  GameProcess game;
  bool hidden = false;  // the game's window is on screen
  bool shown = false;
  bool primed = false;
  int idle_frames = 0;
  float scale = 0.0f;
  for (;;) {
    // Play: the game starts, and the launcher stays.
    if (app.result() == App::Result::kPlay) {
      if (!save()) {
        ShowError(Formatted(t.save_failed, launcher::game::kOptionsFile).c_str());
      }
      // The controllers are the game's while it runs (both reading a DualSense would also set its lights twice).
      ImGui_ImplSDL3_SetGamepadMode(ImGui_ImplSDL3_GamepadMode_Manual, nullptr, 0);
      SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
      std::filesystem::path exe = exe_dir / launcher::game::kRecompExecutable;
      bool found = std::filesystem::exists(exe);
      if (!found) {
        ShowError(Formatted(t.game_missing, launcher::game::kRecompExecutable, launcher::game::kLauncherName).c_str());
      }
      // The launcher's own options are not the game's.
      if (found && game.Start(exe, ArgumentsWithout({L"--launcher_"}))) {
        app.GameStarted();
      } else {
        SDL_InitSubSystem(SDL_INIT_GAMEPAD);
        ImGui_ImplSDL3_SetGamepadMode(ImGui_ImplSDL3_GamepadMode_AutoFirst, nullptr, 0);
        app.GameExited(0);
      }
    }
    if (app.GameRunning()) {
      if (game.Exited()) {
        // Back: the launcher on the Play page, in front, with the controllers.
        uint32_t exit_code = game.ExitCode();
        game.Close();
        SDL_InitSubSystem(SDL_INIT_GAMEPAD);
        ImGui_ImplSDL3_SetGamepadMode(ImGui_ImplSDL3_GamepadMode_AutoFirst, nullptr, 0);
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
        // The releases of the key or click that started the game went unseen (the launcher took no input since).
        io.ClearInputKeys();
        io.ClearInputMouse();
        app.GameExited(exit_code);
        if (hidden) {
          // Shown without activating it: activated while another window still had the foreground, SDL would not
          // see the activation as its focus (its keyboard events then name no window, and ImGui drops them).
          // BringToFront activates it with the foreground.
          SDL_SetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN, "0");
          SDL_ShowWindow(window);
          SDL_ResetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN);
          hidden = false;
        }
        BringToFront(hwnd);
        // The button that quit the game may still be held: nothing is pressed until all are up.
        primed = false;
        idle_frames = 0;
      } else if (!hidden && game.WindowShown()) {
        SDL_HideWindow(window);
        hidden = true;
      }
      if (hidden) {
        // Nothing to draw: sleep until the game exits, answering Windows' messages meanwhile.
        HANDLE process = game.handle();
        MsgWaitForMultipleObjects(1, &process, FALSE, INFINITE, QS_ALLINPUT);
        SDL_PumpEvents();
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
        continue;
      }
    }

    // Nothing changed for a while: wait for input instead of drawing the same frame.
    if (idle_frames > 30 && !app.Busy()) {
      SDL_WaitEventTimeout(nullptr, 250);
    }
    bool input = false;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      input = true;
      if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        ui::gfx::Resize(event.window.data1, event.window.data2);
      }
      // While the game starts the launcher takes no input, and cannot be closed (the game would close with it).
      if (app.GameRunning()) {
        continue;
      }
      if (event.type == SDL_EVENT_QUIT ||
          (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window))) {
        app.Quit();
        break;
      }
      if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && !app.Listening() &&
          (event.key.key == SDLK_F11 || (event.key.key == SDLK_RETURN && (event.key.mod & SDL_KMOD_ALT)))) {
        fullscreen = !fullscreen;
        SDL_SetWindowFullscreen(window, fullscreen);
        if (!SteamWantsFullscreen()) {
          app.settings().o.ui_fullscreen = fullscreen;
          launcher::SaveOptions(app.settings());
        }
        continue;
      }
      if (app.OnEvent(event)) {
        continue;
      }
      ImGui_ImplSDL3_ProcessEvent(&event);
    }
    idle_frames = input ? 0 : idle_frames + 1;
    if (app.result() == App::Result::kQuit || app.result() == App::Result::kRestart) {
      break;
    }
    if (app.result() == App::Result::kPlay) {
      continue;
    }
    if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
      SDL_WaitEventTimeout(nullptr, 100);
      continue;
    }

    int pixel_width = 0;
    int pixel_height = 0;
    SDL_GetWindowSizeInPixels(window, &pixel_width, &pixel_height);
    if (float s = ScaleFor(pixel_width, pixel_height); s != scale) {
      scale = s;
      ui::SetScale(scale);
      ui::ReleasePictures();
    }
    // The controller navigates once the buttons held at the start are up, and not while a key is being
    // assigned (B cancels that, App::OnKeyCapture).
    // A controller button held when the window opened (the A that started the launcher from Big Picture) must
    // not press anything; neither must the one just assigned on the Gamepad page (App::GamepadLocked), nor the
    // one that quit the game.
    if (!primed && !ui::AnyGamepadButtonDown()) {
      primed = true;
    }
    if (primed && !app.Listening() && !app.GamepadLocked() && !app.GameRunning()) {
      io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    } else {
      io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    app.Frame(ImVec2(float(pixel_width), float(pixel_height)));
    ImGui::Render();
    ui::gfx::BeginFrame(ui::col::kBackground);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    ui::gfx::Present();
    if (!shown) {
      SDL_ShowWindow(window);
      SDL_RaiseWindow(window);
      shown = true;
    }
  }

  save();
  // After an update: the new launcher, in place of this one. This process waits for it (and is gone only then), so
  // that Steam still sees the program it started running.
  GameProcess next;
  bool restarted = false;
  if (app.result() == App::Result::kRestart) {
    std::wstring arguments = ArgumentsWithout({L"--launcher_updated_from", L"--launcher_first_run"});
    std::string from = app.UpdatedFromArgument();
    arguments += L" " + std::wstring(from.begin(), from.end());
    restarted = next.Start(exe_dir / launcher::game::kLauncherExecutable, arguments);
  }
  app.ReleaseTextures();
  ui::ReleasePictures();
  ImGui_ImplDX11_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();
  ui::gfx::Shutdown();
  SDL_DestroyWindow(window);
  SDL_Quit();
  if (restarted) {
    WaitForSingleObject(next.handle(), INFINITE);
    return int(next.ExitCode());
  }
  return 0;
}
