// The launcher's window contents: the sidebar and the pages of Gustavo's mockup (made for POTCAWERecomp),
// on the settings of settings.h. main.cpp owns the window and the loop.

#pragma once

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_video.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "achievements.h"
#include "install.h"
#include "updater.h"
#include "settings.h"
#include "texts.h"
#include "ui/ui.h"

namespace launcher {

// The launcher's version (res/version.h), shown in the sidebar and the Updates page.
extern const char* const kVersion;

// Opens a web page in the default browser.
void OpenUrl(const char* url);
// Opens a folder in Explorer.
void OpenFolder(const std::filesystem::path& folder);


// kGame only when the game has options of its own (game::kHasGamePage).
enum class Page : int { kPlay, kAchievements, kKeyboard, kGamepad, kVideo, kGraphics, kGame, kUpdates, kCount };

class App {
 public:
  // kRestart: an update installed the launcher's new version, which main.cpp starts in place of this one.
  enum class Result { kNone, kPlay, kQuit, kRestart };

  App(Settings settings, uint32_t language);

  // The whole window, between ImGui::NewFrame() and ImGui::Render(). `size` in pixels.
  void Frame(ImVec2 size);
  // An SDL event before ImGui sees it; true when the launcher used it (a key being assigned).
  bool OnEvent(const SDL_Event& event);

  Result result() const { return result_; }
  void Quit() { result_ = Result::kQuit; }
  // Something is changing on screen (a key being assigned, a message): keep drawing.
  bool Busy() const;
  // A key is being assigned: the controller does not navigate meanwhile (B cancels).
  bool Listening() const { return listen_bind_ >= 0 || pad_listen_ >= 0; }
  // A controller button was just assigned: the controller does nothing until its buttons are up.
  bool GamepadLocked() const { return pad_release_wait_; }
  Settings& settings() { return settings_; }
  // The window, for the file dialog.
  void SetWindow(SDL_Window* window) { window_ = window; }
  const Texts& texts() const { return t_; }
  // Before the graphics device goes: the achievement icons' textures.
  void ReleaseTextures();

  // Play started the game (main.cpp): until its window shows, the launcher draws "Starting the game…" over its
  // page and takes no input.
  void GameStarted();
  bool GameRunning() const { return game_running_; }
  // The game exited (the launcher shows again): back on the Play page with PLAY focused, the achievements read
  // again, and a notice when the exit code is not 0.
  void GameExited(uint32_t exit_code);

  // The arguments of the launcher's next start after an update (main.cpp): it names the version it came from.
  std::string UpdatedFromArgument() const;

 private:
  // A setting changed: the options file (game::kOptionsFile) is written at once.
  void Changed();

  void Sidebar(const ImRect& rect);
  void ControllerHints(ImVec2 pos, float width);

  // Pages, each in its own file (page_*.cpp). `rect` is the area right of the sidebar.
  void PlayPage(const ImRect& rect);
  void AchievementsPage(const ImRect& rect);
  void KeyboardPage(const ImRect& rect);
  void GamepadPage(const ImRect& rect);
  void VideoPage(const ImRect& rect);
  void GraphicsPage(const ImRect& rect);
  void GamePage(const ImRect& rect);
  void UpdatesPage(const ImRect& rect);
  // The release notes (Markdown) at `pos`, `width` wide. Returns the height; draws only when `draw`.
  float ReleaseNotes(const std::string& markdown, ImVec2 pos, float width, bool draw);
  // A notice across the top of the Play page's art (an error exit code, an update installed).
  // `link_page` kCount: the link opens the logs folder.
  void Notice(const ImRect& rect, bool error, const char* text, const char* link, Page link_page);
  // When updates were last checked ("22/10/2026 18:04"), or "never".
  std::string LastCheckedText() const;

  // The pages' shared pieces. Each returns the y below it (pixels).
  float Header(ImVec2 pos, const char* kicker, const char* title, const char* subtitle);
  // A settings row in a card: the title and hint on the left, the control at `*control` (its height in
  // dp is `control_height`). Returns the row's height.
  float Row(ImVec2 pos, float width, float padding_y, const char* title, const char* hint, float control_height,
            bool last, ImVec2* control);
  // "Changes are saved automatically" and a reset button at the bottom of `rect`; true when reset.
  bool Footer(const ImRect& rect, const char* reset_label);

  void Navigate(Page page, bool focus_sidebar);
  void StartingOverlay(ImVec2 size);
  void FocusSidebar() { focus_sidebar_ = true; }

  // The keyboard page's key being assigned (page_keyboard.cpp).
  bool OnKeyCapture(const SDL_Event& event);
  // The Gamepad page's control being assigned a button (page_gamepad.cpp).
  bool OnPadCapture(const SDL_Event& event);
  void AssignPad(int input);
  void StartListening(int bind, int slot);
  void StopListening();
  void Assign(std::string token);

  Settings settings_;
  const Texts& t_;
  Page page_ = Page::kPlay;
  Result result_ = Result::kNone;
  bool focus_sidebar_ = false;
  bool gamepad_used_ = false;  // the last input came from a controller: the hints show
  ImGuiID page_focus_ = 0;     // the control the controller enters the page on (App::Frame)
  bool focus_page_ = false;    // focus page_focus_ on the next frame (back from the game)
  bool game_running_ = false;
  uint32_t game_exit_code_ = 0;  // the last game's, when not 0: a notice on the Play page

  // Keyboard page.
  int listen_bind_ = -1;  // the Bind whose key is being assigned, or -1
  int listen_slot_ = 0;   // 0 = primary, 1 = secondary
  uint8_t held_modifiers_ = 0;
  bool modifier_pressed_ = false;
  std::string notice_;  // a message under the keyboard page's title, for a moment
  std::chrono::steady_clock::time_point notice_until_{};

  // Gamepad page.
  int selected_pad_ = 0;
  SDL_GamepadType pad_type_ = SDL_GAMEPAD_TYPE_UNKNOWN;  // the chosen controller's type: its layout is edited
  int pad_listen_ = -1;  // the control waiting for a button, or -1
  bool pad_release_wait_ = false;

  // Achievements page.
  int achievement_filter_ = 0;

  // The achievements (achievements.h): read on a thread when the launcher opens, and once the first run has
  // extracted the game; the game is asked to write their list when it is missing.
  void LoadAchievements();
  void PollAchievements();
  // The icon of achievements_->items[index], made into a texture the first time it is drawn.
  ImTextureID AchievementIcon(size_t index);
  void AchievementsCard(const ImRect& rect, float link_y);
  uint32_t language_;
  std::future<std::optional<Achievements>> achievements_loading_;
  std::optional<Achievements> achievements_;
  std::vector<ImTextureID> achievement_icons_;
  bool achievements_requested_ = false;

  // Updates (updater.h): checked on a thread when the launcher opens (if allowed) and on request.
  void CheckForUpdatesNow();
  void PollUpdates();
  std::string update_url_;     // game::kReleasesApi, or --launcher_update_url for a test; empty: no updates
  std::string updated_from_;   // --launcher_updated_from: this start follows an update from that version
  std::future<UpdateCheck> update_check_;
  std::optional<UpdateCheck> updates_;
  Update update_;

  // The first run (page_install.cpp): the game's files extracted from the player's ISO.
  enum class InstallStep { kWelcome, kChooseIso, kExtracting, kDone };
  struct DialogResult {
    std::mutex mutex;
    std::optional<std::string> path;  // UTF-8
  };
  void InstallScreen(ImVec2 size);
  void SetStep(InstallStep step) {
    install_step_ = step;
    step_changed_ = true;
  }
  void InstallArt(const ImRect& rect);
  void Stepper(ImVec2 pos);
  void ChooseIso(const std::string& path);
  void BrowseIso();
  void PollInstall();
  std::string Gigabytes(uint64_t bytes) const;
  std::filesystem::path exe_dir_;
  bool installing_ = false;
  InstallStep install_step_ = InstallStep::kWelcome;
  bool step_changed_ = true;
  std::string iso_pending_;  // the file being checked, UTF-8
  std::future<IsoCheck> iso_check_;
  std::optional<IsoCheck> iso_;
  std::shared_ptr<DialogResult> dialog_ = std::make_shared<DialogResult>();
  Extraction extraction_;
  std::string install_error_;  // not enough space, under the ISO card
  std::chrono::steady_clock::time_point rate_time_{};
  uint64_t rate_done_ = 0;
  double rate_ = 0.0;  // bytes per second, smoothed
  SDL_Window* window_ = nullptr;
};

}  // namespace launcher
