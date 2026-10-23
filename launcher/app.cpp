// The launcher's window contents (app.h): the sidebar, the pages' shared pieces and the
// controller's shortcuts.

#include "app.h"

#include <SDL3/SDL_messagebox.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iterator>

#include "game.h"
#include "ui/gfx.h"
#include "version.h"

namespace launcher {

using namespace ui;

const char* const kVersion = game::kVersion;

namespace {

// Where the Donate button leads; the sidebar shows its card only when there is one (Gustavo, 2026-10-22:
// pending).
constexpr const char* kDonateUrl = "";

// The sidebar's entries, top to bottom.
struct NavEntry {
  Page page;     // the page it opens
  bool sub;      // a page under a group (smaller, indented, no icon)
  bool group;    // a group's header: opens its first page
  Icon icon;
  float margin;  // above it, in dp
};

constexpr NavEntry kNav[] = {
    {Page::kPlay, false, false, Icon::kPlay, 0},
    {Page::kAchievements, false, false, Icon::kTrophy, 0},
    {Page::kKeyboard, false, true, Icon::kGamepad, 10},
    {Page::kKeyboard, true, false, Icon::kNone, 0},
    {Page::kGamepad, true, false, Icon::kNone, 0},
    {Page::kVideo, false, true, Icon::kSliders, 10},
    {Page::kVideo, true, false, Icon::kNone, 0},
    {Page::kGraphics, true, false, Icon::kNone, 0},
    {Page::kGame, true, false, Icon::kNone, 0},
    {Page::kUpdates, false, false, Icon::kRefresh, 10},
};

bool InGroup(Page group_first, Page page) {
  if (group_first == Page::kKeyboard) return page == Page::kKeyboard || page == Page::kGamepad;
  if (group_first == Page::kVideo) return page == Page::kVideo || page == Page::kGraphics || page == Page::kGame;
  return false;
}

}  // namespace

namespace {

// The value of a launcher option on the command line (--launcher_update_url=...), or empty.
std::string CommandLineValue(const std::wstring& name) {
  std::wstring line = GetCommandLineW();
  size_t at = line.find(name);
  if (at == std::wstring::npos) return {};
  at += name.size();
  bool quoted = at < line.size() && line[at] == L'"';
  if (quoted) ++at;
  size_t end = line.find(quoted ? L'"' : L' ', at);
  std::wstring value = line.substr(at, end == std::wstring::npos ? std::wstring::npos : end - at);
  int length = WideCharToMultiByte(CP_UTF8, 0, value.data(), int(value.size()), nullptr, 0, nullptr, nullptr);
  std::string out(size_t(length), '\0');
  WideCharToMultiByte(CP_UTF8, 0, value.data(), int(value.size()), out.data(), length, nullptr, nullptr);
  return out;
}

}  // namespace

void OpenFolder(const std::filesystem::path& folder) {
  ShellExecuteW(nullptr, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void OpenUrl(const char* url) {
  // SDL_OpenURL is not in the SDK's SDL build (SDL_MISC off).
  int length = MultiByteToWideChar(CP_UTF8, 0, url, -1, nullptr, 0);
  std::wstring wide(size_t(std::max(length, 1)), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, url, -1, wide.data(), length);
  ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

App::App(Settings settings, uint32_t language)
    : settings_(std::move(settings)),
      t_(TextsFor(language)),
      language_(language),
      exe_dir_(settings_.options_path.parent_path()) {
  // The first run: the game's files are not there yet, or --launcher_first_run asks for it (to install again, or
  // to see it in a development build, whose game falls back to the repository's files).
  installing_ = !GameInstalled(exe_dir_) || std::wstring(GetCommandLineW()).find(L"--launcher_first_run") != std::wstring::npos;
  if (!installing_) {
    LoadAchievements();
  }
  // Updates: what an update left is cleaned up, and a new version is looked for if allowed.
  update_url_ = CommandLineValue(L"--launcher_update_url=");
  if (update_url_.empty()) {
    update_url_ = game::kReleasesApi;
  }
  updated_from_ = CommandLineValue(L"--launcher_updated_from=");
  CleanUpAfterUpdate(exe_dir_);
  if (!installing_ && settings_.o.check_updates && !update_url_.empty()) {
    CheckForUpdatesNow();
  }
  // PLAY has the focus from the first frame (ImGui::SetItemDefaultFocus in PlayPage does not show it).
  focus_page_ = true;
}

bool App::Busy() const {
  Update::State update = update_.Get().state;
  bool updating = update == Update::State::kDownloading || update == Update::State::kVerifying ||
                  update == Update::State::kInstalling;
  return listen_bind_ >= 0 || std::chrono::steady_clock::now() < notice_until_ || achievements_loading_.valid() ||
         game_running_ || update_check_.valid() || updating ||
         (installing_ && (iso_check_.valid() || extraction_.Get().state == Extraction::State::kRunning));
}

void App::LoadAchievements() {
  achievements_requested_ = true;
  std::filesystem::path exe_dir = exe_dir_;
  uint32_t language = language_;
  achievements_loading_ = std::async(std::launch::async, [exe_dir, language] {
    std::optional<Achievements> list = launcher::LoadAchievements(exe_dir, language);
    if (!list && WriteAchievementList(exe_dir)) {
      list = launcher::LoadAchievements(exe_dir, language);
    }
    return list;
  });
}

void App::PollAchievements() {
  if (!achievements_loading_.valid() ||
      achievements_loading_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
    return;
  }
  std::optional<Achievements> list = achievements_loading_.get();
  if (list) {
    ReleaseTextures();
    achievements_ = std::move(list);
    achievement_icons_.assign(achievements_->items.size(), ImTextureID_Invalid);
  }
}

ImTextureID App::AchievementIcon(size_t index) {
  if (!achievements_ || index >= achievement_icons_.size()) {
    return ImTextureID_Invalid;
  }
  Achievement& a = achievements_->items[index];
  if (achievement_icons_[index] == ImTextureID_Invalid && !a.icon.empty()) {
    achievement_icons_[index] = ui::gfx::CreateTexture(a.icon.data(), a.icon_width, a.icon_height);
    a.icon.clear();  // on the GPU now
    a.icon.shrink_to_fit();
  }
  return achievement_icons_[index];
}

void App::ReleaseTextures() {
  for (ImTextureID& texture : achievement_icons_) {
    ui::gfx::DestroyTexture(texture);
    texture = ImTextureID_Invalid;
  }
}

void App::Changed() {
  static bool reported = false;
  if (!SaveOptions(settings_) && !reported) {
    reported = true;
    char text[512];
    std::snprintf(text, sizeof(text), t_.save_failed, game::kOptionsFile);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, game::kLauncherName, text, nullptr);
  }
}

void App::CheckForUpdatesNow() {
  if (update_check_.valid() || update_url_.empty()) return;
  std::string url = update_url_;
  update_check_ = std::async(std::launch::async, [url] { return CheckForUpdates(url); });
}

void App::PollUpdates() {
  if (update_check_.valid() && update_check_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
    updates_ = update_check_.get();
    if (updates_->ok) {
      settings_.o.last_update_check = int64_t(std::time(nullptr));
      Changed();
    }
  }
  // Installed: main.cpp starts the new launcher.
  if (update_.Get().state == Update::State::kDone && result_ == Result::kNone) {
    result_ = Result::kRestart;
  }
}

std::string App::UpdatedFromArgument() const {
  return std::string("--launcher_updated_from=") + kVersion;
}

void App::GameStarted() {
  result_ = Result::kNone;
  game_running_ = true;
  game_exit_code_ = 0;
}

void App::GameExited(uint32_t exit_code) {
  result_ = Result::kNone;
  game_running_ = false;
  game_exit_code_ = exit_code;
  Navigate(Page::kPlay, false);
  focus_page_ = true;
  // The game may have unlocked some, or written the list for the first time.
  LoadAchievements();
}

void App::StartingOverlay(ImVec2 size) {
  // A window over the page, which takes no input (main.cpp does not pass it any meanwhile).
  ImVec2 origin = ImGui::GetMainViewport()->Pos;
  ImGui::SetNextWindowPos(origin);
  ImGui::SetNextWindowSize(size);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::Begin("##starting", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav);
  ImGui::PopStyleVar(2);
  ImDrawList* draw = ImGui::GetWindowDrawList();
  draw->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), Fade(col::kBackground, 0.88f));
  constexpr Type kText{Font::kSemiBold, 18};
  ImVec2 center(origin.x + size.x * 0.5f, origin.y + size.y * 0.5f);
  // A spinning arc above the text.
  float radius = Px(18.0f);
  ImVec2 spinner(center.x, center.y - Px(28.0f));
  float start = float(ImGui::GetTime()) * 5.0f;
  draw->PathArcTo(spinner, radius, 0.0f, 6.2831853f, 48);
  draw->PathStroke(col::kTrack, ImDrawFlags_None, Px(3.0f));
  draw->PathArcTo(spinner, radius, start, start + 1.9f, 24);
  draw->PathStroke(col::kAccent, ImDrawFlags_None, Px(3.0f));
  ImVec2 text_size = MeasureText(kText, t_.starting_game);
  ImVec2 text_pos(Snap(center.x - text_size.x * 0.5f), Snap(spinner.y + radius + Px(22.0f)));
  DrawLabel(kText, text_pos, col::kText, t_.starting_game);
  ImGui::End();
}

void App::Navigate(Page page, bool focus_sidebar) {
  StopListening();
  page_ = page;
  if (focus_sidebar) {
    focus_sidebar_ = true;
  }
}

void App::Frame(ImVec2 size) {
  PollAchievements();
  PollUpdates();
  ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->Pos);
  ImGui::SetNextWindowSize(size);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::Begin("##launcher", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus |
                   ImGuiWindowFlags_NoScrollWithMouse);
  ImGui::PopStyleVar(3);

  if (installing_) {
    InstallScreen(size);
    ImGui::End();
    return;
  }

  ImVec2 origin = viewport->Pos;
  float sidebar = Snap(Px(232.0f));
  // The sidebar is a child window of its own, so ImGui's directional navigation does not mix its entries
  // with the page's controls: its scoring prefers an item a pixel lower over one level with the focus, and
  // the sidebar's rows won over the button next door. The controller crosses between the two below.
  ImGui::SetCursorScreenPos(origin);
  ImGui::BeginChild("##sidebar", ImVec2(sidebar, size.y), ImGuiChildFlags_None,
                    ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  ImGuiWindow* sidebar_window = ImGui::GetCurrentWindow();
  Sidebar(ImRect(origin, ImVec2(origin.x + sidebar, origin.y + size.y)));
  ImGui::EndChild();

  ImRect main(ImVec2(origin.x + sidebar, origin.y), ImVec2(origin.x + size.x, origin.y + size.y));
  page_focus_ = 0;
  CaptureFirstItem();
  switch (page_) {
    case Page::kPlay:
      PlayPage(main);
      break;
    case Page::kAchievements:
      AchievementsPage(main);
      break;
    case Page::kKeyboard:
      KeyboardPage(main);
      break;
    case Page::kGamepad:
      GamepadPage(main);
      break;
    case Page::kVideo:
      VideoPage(main);
      break;
    case Page::kGraphics:
      GraphicsPage(main);
      break;
    case Page::kGame:
      GamePage(main);
      break;
    case Page::kUpdates:
      UpdatesPage(main);
      break;
    case Page::kCount:
      break;
  }
  // The control a controller enters the page on: the page's choice (PLAY), or its first.
  ImGuiID first = CapturedFirstItem();
  if (!page_focus_) {
    page_focus_ = first;
  }

  ImGuiContext& g = *GImGui;
  // Left from the page lands on the sidebar as a whole (ImGui treats a child window as one item of its parent):
  // into it, on the current page's entry. Right in the sidebar enters the page.
  if (g.NavId == sidebar_window->ChildId) {
    FocusSidebar();
  } else if (g.NavWindow == sidebar_window && page_focus_ &&
             (ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight) ||
              ImGui::IsKeyPressed(ImGuiKey_GamepadLStickRight))) {
    ImGui::SetFocusID(page_focus_, ImGui::GetCurrentWindow());
    ImGui::SetNavCursorVisible(true);
  }

  // The controller's shortcuts: LB/RB change the page, Start plays, B returns to the sidebar. Not while a
  // list is open (B closes it) or a key is being assigned.
  static bool popup_was_open = false;
  bool popup = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
  // The controller's buttons are ImGui's state, not events: one just assigned on the Gamepad page still looks
  // pressed. It does nothing until all are up.
  if (pad_release_wait_ && !AnyGamepadButtonDown()) {
    pad_release_wait_ = false;
  }
  if (!popup && !popup_was_open && !Listening() && !pad_release_wait_) {
    constexpr Page kOrder[] = {Page::kPlay,     Page::kAchievements, Page::kKeyboard, Page::kGamepad,
                               Page::kVideo,    Page::kGraphics,     Page::kGame,     Page::kUpdates};
    int index = int(std::find(std::begin(kOrder), std::end(kOrder), page_) - std::begin(kOrder));
    int count = int(std::size(kOrder));
    // The next page in `step`'s direction that the game has.
    auto next = [&](int step) {
      int i = index;
      do {
        i = (i + count + step) % count;
      } while (kOrder[i] == Page::kGame && !game::kHasGamePage);
      return kOrder[i];
    };
    if (ImGui::IsKeyPressed(ImGuiKey_GamepadL1)) {
      Navigate(next(-1), true);
    } else if (ImGui::IsKeyPressed(ImGuiKey_GamepadR1)) {
      Navigate(next(1), true);
    } else if (ImGui::IsKeyPressed(ImGuiKey_GamepadStart, false)) {
      result_ = Result::kPlay;
    } else if (page_ == Page::kPlay && !g.NavCursorVisible &&
               (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))) {
      // PLAY is the page's default button: Enter plays while no control shows the focus (ImGui would not activate
      // it then).
      result_ = Result::kPlay;
    } else if (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false) || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
      FocusSidebar();
    }
  }
  popup_was_open = popup;

  // When the launcher opens and back from the game: PLAY has the focus, shown when a controller is connected, so
  // that A (or Enter) plays at once (ImGui activates only an item whose focus is shown).
  if (focus_page_ && page_focus_) {
    ImGui::SetFocusID(page_focus_, ImGui::GetCurrentWindow());
    ImGui::SetNavCursorVisible(gamepad_used_ || SDL_HasGamepad());
    focus_page_ = false;
  }

  ImGui::End();
  if (game_running_) {
    StartingOverlay(size);
  }
}

bool App::OnEvent(const SDL_Event& event) {
  switch (event.type) {
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
      if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN ||
          std::abs(int(event.gaxis.value)) > 16000) {
        gamepad_used_ = true;
      }
      break;
    case SDL_EVENT_MOUSE_MOTION:
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_KEY_DOWN:
      gamepad_used_ = false;
      break;
    case SDL_EVENT_DROP_FILE:
      // An ISO dropped on the window, on the first run's ISO step.
      if (installing_ && install_step_ == InstallStep::kChooseIso && event.drop.data) {
        ChooseIso(event.drop.data);
      }
      return true;
    default:
      break;
  }
  return OnKeyCapture(event) || OnPadCapture(event);
}

// --- The sidebar ---

void App::Sidebar(const ImRect& rect) {
  FillRect(rect, col::kSidebar);
  FillRect(ImRect(ImVec2(rect.Max.x - Snap(Px(1.0f)), rect.Min.y), rect.Max), col::kSidebarBorder);
  float x = rect.Min.x + Px(16.0f);
  float width = rect.GetWidth() - Px(32.0f) - Snap(Px(1.0f));
  float y = rect.Min.y + Px(24.0f);

  // The brand: the game's mark on accent and its name.
  ImRect logo(ImVec2(x + Px(8.0f), y), ImVec2(x + Px(48.0f), y + Px(40.0f)));
  FillRect(logo, col::kAccent, 8.0f);
  DrawIcon(Icon::kBrand, ImVec2(logo.Min.x + Px(9.0f), logo.Min.y + Px(9.0f)), 22.0f, col::kOnAccent, 2.0f);
  constexpr Type kBrandTitle{Font::kCondensedBold, 20};
  constexpr Type kBrandSubtitle{Font::kSemiBold, 11, 0.12f};
  float block = Px(20.0f) + Px(5.0f) + LineHeight(kBrandSubtitle);
  float text_y = y + (Px(40.0f) - block) * 0.5f;
  DrawLabel(kBrandTitle, ImVec2(logo.Max.x + Px(12.0f), text_y), col::kText, game::kArtTitle, Px(20.0f));
  DrawLabel(kBrandSubtitle, ImVec2(logo.Max.x + Px(12.0f), text_y + Px(25.0f)), col::kLabel, t_.brand);
  y += Px(40.0f) + Px(28.0f);

  // The pages.
  const char* labels[] = {t_.nav_play,     t_.nav_achievements, t_.nav_controls, t_.nav_keyboard, t_.nav_gamepad,
                          t_.nav_settings, t_.nav_video,        t_.nav_graphics, t_.nav_game,     t_.nav_updates};
  ImGuiID current_id = 0;
  for (int i = 0; i < int(std::size(kNav)); ++i) {
    const NavEntry& entry = kNav[i];
    if (entry.page == Page::kGame && !game::kHasGamePage) {
      continue;
    }
    if (i > 0) {
      y += Px(2.0f) + Px(entry.margin);
    }
    float height = Px(entry.sub ? 40.0f : 44.0f);
    ImRect item(ImVec2(x, y), ImVec2(x + width, y + height));
    ImGui::PushID(i);
    ImGuiID id = ImGui::GetID("nav");
    ItemState state = Item(id, item);
    ImGui::PopID();
    bool active = !entry.group && page_ == entry.page;
    bool parent = entry.group && InGroup(entry.page, page_);
    if (active) {
      current_id = id;
    }
    ImU32 background = active ? col::kAccentTint : 0;
    ImU32 text = active ? col::kAccentLight : parent ? col::kText : col::kTextSecondary;
    ImU32 icon = active || parent ? col::kAccent : col::kTextDim;
    if (state.hovered) {
      background = background ? Brighten(background) : 0;
      text = Brighten(text);
      icon = Brighten(icon);
    }
    if (background) {
      FillRect(item, background, 8.0f);
    }
    if (entry.sub) {
      DrawLabel(Type{Font::kMedium, 14}, ImVec2(item.Min.x + Px(44.0f), item.Min.y), text, labels[i], height);
    } else {
      DrawIcon(entry.icon, ImVec2(item.Min.x + Px(12.0f), item.Min.y + (height - Px(20.0f)) * 0.5f), 20.0f, icon,
               1.8f);
      DrawLabel(Type{Font::kSemiBold, 15}, ImVec2(item.Min.x + Px(44.0f), item.Min.y), text, labels[i], height);
    }
    // A new version: a dot on Updates.
    if (entry.page == Page::kUpdates && !entry.group && updates_ && updates_->Newer(Version::Parse(kVersion))) {
      float dot = Px(8.0f);
      ImVec2 center(item.Max.x - Px(16.0f), item.GetCenter().y);
      FillRect(ImRect(ImVec2(center.x - dot * 0.5f, center.y - dot * 0.5f), ImVec2(center.x + dot * 0.5f, center.y + dot * 0.5f)),
               col::kAccent, 4.0f);
    }
    if (state.focused) {
      FocusRing(item, 8.0f);
    }
    if (state.pressed) {
      Navigate(entry.page, false);
    }
    y += height;
  }
  if (focus_sidebar_ && current_id) {
    ImGui::SetFocusID(current_id, ImGui::GetCurrentWindow());
    ImGui::SetNavCursorVisible(true);
    focus_sidebar_ = false;
  }

  // From the bottom: the version, the controller's hints, the donation card.
  constexpr Type kVersionType{Font::kRegular, 12};
  float bottom = rect.Max.y - Px(20.0f);
  char version[64];
  std::snprintf(version, sizeof(version), t_.version, kVersion);
  DrawLabel(kVersionType, ImVec2(x + Px(8.0f), bottom - LineHeight(kVersionType)), col::kTextDim, version);
  bottom -= LineHeight(kVersionType) + Px(14.0f);
  if (gamepad_used_) {
    float hints = Px(4.0f * 26.0f + 3.0f * 6.0f + 32.0f);
    ControllerHints(ImVec2(x, bottom - hints), width);
    bottom -= hints + Px(12.0f);
  }
  if (*kDonateUrl) {
    constexpr Type kDonateText{Font::kRegular, 13};
    float text_height = DrawParagraph(kDonateText, ImVec2(0, 0), width - Px(32.0f), 1.4f, 0, t_.donate_text, false);
    float card_height = Px(16.0f) * 2.0f + text_height + Px(12.0f) + Px(44.0f);
    ImRect card(ImVec2(x, bottom - card_height), ImVec2(x + width, bottom));
    FillRect(card, col::kCardRaised, 10.0f);
    DrawParagraph(kDonateText, ImVec2(card.Min.x + Px(16.0f), card.Min.y + Px(16.0f)), width - Px(32.0f), 1.4f,
                    col::kTextSecondary, t_.donate_text);
    ButtonSpec donate{ButtonKind::kDonate, Type{Font::kBold, 15}, 44, 12, 8, Icon::kHeart, 18, 0, true};
    if (Button("donate", ImVec2(card.Min.x + Px(16.0f), card.Max.y - Px(16.0f) - Px(44.0f)), donate, t_.donate, true,
               width - Px(32.0f))) {
      OpenUrl(kDonateUrl);
    }
  }
}

// The controller's buttons for this window, while a controller is in use.
void App::ControllerHints(ImVec2 pos, float width) {
  float height = Px(4.0f * 26.0f + 3.0f * 6.0f + 32.0f);
  FillRect(ImRect(pos, ImVec2(pos.x + width, pos.y + height)), col::kCardRaised, 10.0f);
  constexpr Type kGlyph{Font::kBold, 11};
  constexpr Type kLabel{Font::kMedium, 13};
  struct Hint {
    const char* glyphs[2];
    ImU32 color;
    const char* label;
  };
  const Hint hints[] = {{{"LB", "RB"}, col::kTextSoft, t_.hint_section},
                        {{"A", nullptr}, col::kGlyphA, t_.hint_select},
                        {{"B", nullptr}, col::kGlyphB, t_.hint_back},
                        {{"START", nullptr}, col::kTextSoft, t_.hint_play}};
  float y = pos.y + Px(16.0f);
  for (const Hint& hint : hints) {
    float x = pos.x + Px(16.0f);
    for (const char* glyph : hint.glyphs) {
      if (!glyph) continue;
      float glyph_width = std::max(Px(26.0f), MeasureText(kGlyph, glyph).x + Px(14.0f));
      ImRect pill(ImVec2(x, y), ImVec2(x + glyph_width, y + Px(26.0f)));
      FillRect(pill, col::kGlyph, 13.0f);
      DrawLabel(kGlyph, ImVec2(pill.Min.x + (glyph_width - MeasureText(kGlyph, glyph).x) * 0.5f, pill.Min.y),
               hint.color, glyph, pill.GetHeight());
      x += glyph_width + Px(4.0f);
    }
    DrawLabel(kLabel, ImVec2(pos.x + Px(80.0f), y), col::kTextSecondary, hint.label, Px(26.0f));
    y += Px(26.0f) + Px(6.0f);
  }
}

// --- Shared pieces ---

float App::Header(ImVec2 pos, const char* kicker, const char* title, const char* subtitle) {
  constexpr Type kKicker{Font::kSemiBold, 13};
  constexpr Type kTitle{Font::kCondensedBold, 40};
  constexpr Type kSubtitle{Font::kRegular, 14};
  float y = pos.y;
  if (kicker) {
    DrawLabel(kKicker, ImVec2(pos.x, y), col::kLabel, kicker);
    y += LineHeight(kKicker) + Px(4.0f);
  }
  DrawLabel(kTitle, ImVec2(pos.x, y), col::kText, title, Px(40.0f));
  y += Px(40.0f);
  if (subtitle) {
    y += Px(8.0f);
    DrawLabel(kSubtitle, ImVec2(pos.x, y), col::kTextMuted, subtitle);
    y += LineHeight(kSubtitle);
  }
  return y;
}

float App::Row(ImVec2 pos, float width, float padding_y, const char* title, const char* hint, float control_height,
               bool last, ImVec2* control) {
  constexpr Type kTitle{Font::kSemiBold, 15};
  constexpr Type kHint{Font::kRegular, 13};
  float label_height = LineHeight(kTitle) + Px(4.0f) + LineHeight(kHint);
  float content = std::max(label_height, Px(control_height));
  float top = pos.y + Px(padding_y);
  float label_y = top + (content - label_height) * 0.5f;
  float label_width = Px(300.0f);
  DrawLabel(kTitle, ImVec2(pos.x + Px(28.0f), label_y), col::kText, Ellipsize(kTitle, title, label_width));
  DrawLabel(kHint, ImVec2(pos.x + Px(28.0f), label_y + LineHeight(kTitle) + Px(4.0f)), col::kTextMuted,
           Ellipsize(kHint, hint, label_width + Px(20.0f)));
  *control = ImVec2(pos.x + Px(28.0f) + label_width + Px(24.0f), top + (content - Px(control_height)) * 0.5f);
  float height = content + Px(padding_y) * 2.0f;
  if (!last) {
    FillRect(ImRect(ImVec2(pos.x, pos.y + height), ImVec2(pos.x + width, pos.y + height + Snap(Px(1.0f)))),
             col::kDivider);
    height += Snap(Px(1.0f));
  }
  return height;
}

bool App::Footer(const ImRect& rect, const char* reset_label) {
  constexpr Type kNote{Font::kRegular, 13};
  float top = rect.Max.y - Px(44.0f);
  DrawIcon(Icon::kCheck, ImVec2(rect.Min.x, top + Px(14.0f)), 16.0f, col::kSuccess, 2.4f);
  DrawLabel(kNote, ImVec2(rect.Min.x + Px(24.0f), top), col::kTextMuted, t_.autosave, Px(44.0f));
  ButtonSpec spec;
  float width = ButtonWidth(spec, reset_label);
  return Button("reset", ImVec2(rect.Max.x - width, top), spec, reset_label);
}

}  // namespace launcher
