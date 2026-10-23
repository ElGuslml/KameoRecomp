// The first run, Gustavo's mockup's setup screens: Welcome, Choose your ISO (and its
// invalid variant), Extracting, Ready. The launcher opens on them while the game's files are missing
// (GameInstalled); the ISO is checked and extracted by install.cpp, off the UI thread.

#include <SDL3/SDL_dialog.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "app.h"
#include "game.h"

namespace launcher {

using namespace ui;

namespace {

constexpr Type kHeading{Font::kCondensedBold, 48};
constexpr Type kLead{Font::kRegular, 16};
constexpr Type kCardTitle{Font::kSemiBold, 15};
constexpr Type kCardHint{Font::kRegular, 13};

std::string Format(const char* format, const std::string& a, const std::string& b = {}) {
  char text[1024];
  std::snprintf(text, sizeof(text), format, a.c_str(), b.c_str());
  return text;
}

std::string Utf8(const std::filesystem::path& path) {
  std::u8string text = path.u8string();
  return std::string(text.begin(), text.end());
}

// The mockup's footer buttons: 52 dp high.
ButtonSpec FooterButton(ButtonKind kind) {
  ButtonSpec spec;
  spec.kind = kind;
  spec.height = 52;
  spec.radius = 10;
  if (kind == ButtonKind::kPrimary || kind == ButtonKind::kDisabled) {
    spec.type = Type{Font::kBold, 16};
    spec.padding = 32;
  } else {
    spec.type = Type{Font::kSemiBold, 15};
    spec.padding = 22;
  }
  return spec;
}

ButtonSpec WithArrow(ButtonSpec spec) {
  spec.icon = Icon::kArrowRight;
  spec.icon_size = 18;
  spec.icon_stroke = 2.2f;
  spec.icon_after = true;
  spec.gap = 10;
  return spec;
}

}  // namespace

std::string App::Gigabytes(uint64_t bytes) const {
  char text[32];
  std::snprintf(text, sizeof(text), "%.1f", double(bytes) / (1024.0 * 1024.0 * 1024.0));
  std::string out = text;
  std::replace(out.begin(), out.end(), '.', t_.decimal_separator[0]);
  return out;
}

void App::ChooseIso(const std::string& path) {
  if (iso_check_.valid()) {
    return;  // one check at a time
  }
  install_error_.clear();
  iso_.reset();
  std::filesystem::path file(std::u8string(reinterpret_cast<const char8_t*>(path.data()), path.size()));
  iso_pending_ = Utf8(file.filename());
  iso_check_ = std::async(std::launch::async, CheckIso, file);
}

void App::BrowseIso() {
  // SDL keeps the filters until the dialog answers.
  static SDL_DialogFileFilter filters[1];
  filters[0] = {t_.iso_filter, "iso"};
  // The dialog answers on a thread of its own, maybe after this App is gone: the callback holds the result
  // through a shared pointer of its own, and PollInstall takes the path on the UI thread.
  auto* holder = new std::shared_ptr<DialogResult>(dialog_);
  SDL_ShowOpenFileDialog(
      [](void* userdata, const char* const* files, int) {
        auto* result = static_cast<std::shared_ptr<DialogResult>*>(userdata);
        if (files && files[0]) {
          std::lock_guard lock((*result)->mutex);
          (*result)->path = files[0];
        }
        delete result;
      },
      holder, window_, filters, 1, nullptr, false);
}

void App::PollInstall() {
  {
    std::optional<std::string> chosen;
    {
      std::lock_guard lock(dialog_->mutex);
      chosen.swap(dialog_->path);
    }
    if (chosen && install_step_ == InstallStep::kChooseIso) {
      ChooseIso(*chosen);
    }
  }
  if (iso_check_.valid() && iso_check_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
    iso_ = iso_check_.get();
    step_changed_ = true;  // the focus moves to Extract, or to Choose another file
  }
  // The extraction's speed, smoothed, for the time left.
  Extraction::Progress progress = extraction_.Get();
  if (progress.state == Extraction::State::kRunning) {
    auto now = std::chrono::steady_clock::now();
    double seconds = std::chrono::duration<double>(now - rate_time_).count();
    if (rate_time_ == std::chrono::steady_clock::time_point{}) {
      rate_time_ = now;
      rate_done_ = progress.done;
    } else if (seconds >= 0.5) {
      double rate = double(progress.done - rate_done_) / seconds;
      rate_ = rate_ > 0.0 ? rate_ * 0.7 + rate * 0.3 : rate;
      rate_time_ = now;
      rate_done_ = progress.done;
    }
  }
}

// The art panel left of the setup screens, with the game's name: straight on the art with the tagline at its foot, or
// (game.h's kInstallTitleCard) in a card 40 dp from its edges, padded 24 dp, the background at 88 % over the art.
void App::InstallArt(const ImRect& rect) {
  FillRect(rect, col::kInstallArtFill);
  DrawArt(Art::kInstall, rect, ImVec2(0.5f, 1.0f));
  constexpr Type kBrand{Font::kSemiBold, 11, 0.12f};
  constexpr Type kKicker{Font::kBold, 12, 0.16f};
  constexpr Type kFoot{Font::kRegular, 13};
  float x = rect.Min.x + Px(40.0f);
  float y = rect.Min.y + Px(40.0f);
  float width = rect.GetWidth() - Px(80.0f);
  if (game::kInstallTitleCard) {
    float inner = width - Px(48.0f);
    float height = Px(24.0f) + Px(40.0f) + Px(22.0f) + LineHeight(kKicker) + Px(4.0f) + Px(48.0f) * 0.95f + Px(10.0f) +
                   DrawParagraph(kFoot, ImVec2(0, 0), inner, 1.2f, 0, t_.tagline, false) + Px(24.0f);
    FillRect(ImRect(ImVec2(x, y), ImVec2(x + width, y + height)), Fade(col::kBackground, 0.88f), 12.0f);
    x += Px(24.0f);
    y += Px(24.0f);
    width = inner;
  }
  ImRect logo(ImVec2(x, y), ImVec2(x + Px(40.0f), y + Px(40.0f)));
  FillRect(logo, col::kAccent, game::kBrandRound ? 20.0f : 8.0f);
  DrawIcon(Icon::kBrand, ImVec2(logo.Min.x + Px(9.0f), logo.Min.y + Px(9.0f)), 22.0f, col::kOnAccent, 2.0f);
  DrawLabel(kBrand, ImVec2(logo.Max.x + Px(12.0f), y), col::kTextMuted, t_.brand, Px(40.0f));
  y += Px(40.0f) + (game::kInstallTitleCard ? Px(22.0f) : Px(28.0f));
  DrawLabel(kKicker, ImVec2(x, y), col::kAccent, game::kArtKicker);
  y += LineHeight(kKicker) + Px(4.0f);
  DrawLabel(Type{Font::kCondensedBold, 48}, ImVec2(x, y), col::kText, game::kArtTitle, Px(48.0f) * 0.95f);
  if (game::kInstallTitleCard) {
    y += Px(48.0f) * 0.95f + Px(10.0f);
    DrawParagraph(kFoot, ImVec2(x, y), width, 1.2f, col::kTextMuted, t_.tagline);
  } else {
    DrawParagraph(kFoot, ImVec2(x, rect.Max.y - Px(32.0f) - LineHeight(kFoot) * 2.0f), width, 1.2f, col::kTextMuted,
                  t_.tagline);
  }
}

// The four steps along the top: done (a check on accent), current (accent ring), to come (grey).
void App::Stepper(ImVec2 pos) {
  const char* labels[] = {t_.step_welcome, t_.step_iso, t_.step_install, t_.step_done};
  int current = int(install_step_);
  constexpr Type kNumber{Font::kBold, 13};
  float x = pos.x;
  for (int i = 0; i < 4; ++i) {
    // On the last screen every step is done, the last one in bold (the mockup's Ready).
    bool last = install_step_ == InstallStep::kDone;
    bool done = i < current || last;
    bool now = i == current;
    ImRect circle(ImVec2(x, pos.y), ImVec2(x + Px(28.0f), pos.y + Px(28.0f)));
    if (done) {
      FillRect(circle, col::kAccent, 14.0f);
      DrawIcon(Icon::kCheck, ImVec2(circle.Min.x + Px(7.0f), circle.Min.y + Px(7.0f)), 14.0f, col::kOnAccent, 3.0f);
    } else {
      StrokeRect(circle, now ? col::kAccent : col::kDashed, 14.0f, 2.0f);
      char number[4];
      std::snprintf(number, sizeof(number), "%d", i + 1);
      DrawLabel(kNumber, ImVec2(circle.Min.x + (Px(28.0f) - MeasureText(kNumber, number).x) * 0.5f, circle.Min.y),
                now ? col::kAccent : col::kLabel, number, Px(28.0f));
    }
    x += Px(28.0f) + Px(8.0f);
    Type label{now ? Font::kSemiBold : Font::kRegular, 14};
    DrawLabel(label, ImVec2(x, pos.y), now || done ? col::kText : col::kLabel, labels[i], Px(28.0f));
    x += MeasureText(label, labels[i]).x;
    if (i < 3) {
      x += Px(8.0f) + Px(4.0f);
      FillRect(ImRect(ImVec2(x, pos.y + Px(13.0f)), ImVec2(x + Px(24.0f), pos.y + Px(15.0f))),
               done ? col::kAccent : col::kFieldBorder);
      x += Px(24.0f) + Px(12.0f);
    }
  }
}

void App::InstallScreen(ImVec2 size) {
  PollInstall();
  ImVec2 origin = ImGui::GetMainViewport()->Pos;
  ImRect art(origin, ImVec2(origin.x + Snap(Px(440.0f)), origin.y + size.y));
  InstallArt(art);
  ImRect main(ImVec2(art.Max.x + Px(64.0f), origin.y + Px(40.0f)),
              ImVec2(origin.x + size.x - Px(64.0f), origin.y + size.y - Px(40.0f)));
  Stepper(ImVec2(main.Min.x, main.Min.y + Px(6.0f)));

  float footer_y = main.Max.y - Px(52.0f);
  float body_top = main.Min.y + Px(40.0f);
  // The step's body, centered vertically between the stepper and the footer: measured first, then drawn.
  auto centered = [&](float height) { return body_top + std::max(0.0f, (footer_y - body_top - height) * 0.5f); };
  // B or Esc go back a step where there is a Back button.
  bool back_pressed = ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false) || ImGui::IsKeyPressed(ImGuiKey_Escape, false);
  // The step's main button gets the focus when the step appears (the controller starts there).
  auto default_focus = [this] {
    ImGui::SetItemDefaultFocus();
    if (step_changed_) {
      ImGui::SetFocusID(ImGui::GetItemID(), ImGui::GetCurrentWindow());
      step_changed_ = false;
    }
  };

  switch (install_step_) {
    case InstallStep::kWelcome: {
      float width = std::min(main.GetWidth(), Px(600.0f));
      std::string lines[3][2] = {
          {t_.need_iso, Format(t_.need_iso_hint, game::kName)},
          {t_.install_here, Format(t_.install_here_hint, game::kRecompExecutable)},
          {t_.need_space, Format(t_.need_space_hint, Gigabytes(game::kInstalledBytes), Gigabytes(FreeSpace(exe_dir_)))}};
      // The game's own welcome (game.cpp), or the launcher's.
      const game::GameTexts& own = game::GameTextsFor(language_);
      const char* welcome_title = own.welcome_title ? own.welcome_title : t_.welcome_title;
      const char* welcome_text = own.welcome_text ? own.welcome_text : t_.welcome_text;
      float lead = DrawParagraph(kLead, ImVec2(0, 0), width, 1.55f, 0, welcome_text, false);
      float card = Px(16.0f) * 2.0f + std::max(Px(44.0f), LineHeight(kCardTitle) + Px(3.0f) + LineHeight(kCardHint));
      float height = Px(48.0f) + Px(14.0f) + lead + Px(32.0f) + card * 3.0f + Px(24.0f) + Px(20.0f) + LineHeight(kCardHint);
      float y = centered(height);
      float x = main.Min.x;
      DrawLabel(kHeading, ImVec2(x, y), col::kText, welcome_title, Px(48.0f));
      y += Px(48.0f) + Px(14.0f);
      DrawParagraph(kLead, ImVec2(x, y), width, 1.55f, col::kTextSecondary, welcome_text);
      y += lead + Px(32.0f);
      const Icon icons[] = {Icon::kDisc, Icon::kFolder, Icon::kDrive};
      bool enough = FreeSpace(exe_dir_) >= game::kInstalledBytes;
      for (int i = 0; i < 3; ++i) {
        ImRect r(ImVec2(x, y), ImVec2(x + width, y + card));
        FillRect(r, col::kCard, 10.0f);
        ImRect box(ImVec2(r.Min.x + Px(16.0f), r.Min.y + (card - Px(44.0f)) * 0.5f),
                   ImVec2(r.Min.x + Px(60.0f), r.Min.y + (card + Px(44.0f)) * 0.5f));
        FillRect(box, col::kAccentTint, 8.0f);
        DrawIcon(icons[i], ImVec2(box.Min.x + Px(11.0f), box.Min.y + Px(11.0f)), 22.0f, col::kAccent, 1.8f);
        float tx = box.Max.x + Px(16.0f);
        float ty = r.Min.y + (card - LineHeight(kCardTitle) - Px(3.0f) - LineHeight(kCardHint)) * 0.5f;
        DrawLabel(kCardTitle, ImVec2(tx, ty), col::kText, lines[i][0]);
        DrawLabel(kCardHint, ImVec2(tx, ty + LineHeight(kCardTitle) + Px(3.0f)),
                  i == 2 && !enough ? col::kError : col::kTextMuted,
                  Ellipsize(kCardHint, lines[i][1], r.Max.x - Px(16.0f) - tx));
        y += card + Px(12.0f);
      }
      DrawLabel(kCardHint, ImVec2(x, y + Px(8.0f)), col::kLabel, t_.own_copy);

      if (Button("quit", ImVec2(main.Min.x, footer_y), FooterButton(ButtonKind::kGhost), t_.quit)) {
        result_ = Result::kQuit;
      }
      ButtonSpec start = WithArrow(FooterButton(ButtonKind::kPrimary));
      if (Button("start", ImVec2(main.Max.x - ButtonWidth(start, t_.start), footer_y), start, t_.start)) {
        SetStep(InstallStep::kChooseIso);
      }
      default_focus();
      break;
    }

    case InstallStep::kChooseIso: {
      float width = std::min(main.GetWidth(), Px(640.0f));
      std::string text = Format(t_.choose_text, game::kName);
      float lead = DrawParagraph(kLead, ImVec2(0, 0), width, 1.55f, 0, text, false);
      bool checking = iso_check_.valid();
      bool valid = !checking && iso_ && iso_->result == IsoCheck::Result::kValid;
      bool invalid = !checking && iso_ && !valid;
      float card = Px(16.0f) * 2.0f + std::max(Px(44.0f), LineHeight(kCardTitle) + Px(3.0f) + LineHeight(kCardHint) +
                                                             (valid ? Px(6.0f) + LineHeight(kCardHint) : 0.0f));
      // The invalid variant: the reason, the file and what was found in it, and another try.
      const char* error_title = nullptr;
      std::string error_text;
      std::string detected;
      if (invalid) {
        switch (iso_->result) {
          case IsoCheck::Result::kOtherVersion:
            error_title = t_.wrong_version_title;
            error_text = Format(t_.wrong_version_text, game::kName, game::kRegion);
          {
            char crc[16];
            std::snprintf(crc, sizeof(crc), "%08X", iso_->crc);
            detected = Format("%s (CRC %s)", game::kName, crc);
            break;
          }
          case IsoCheck::Result::kOtherGame: {
            error_title = t_.wrong_game_title;
            error_text = Format(t_.wrong_game_text, game::kName);
            char id[32];
            std::snprintf(id, sizeof(id), "Title ID %08X", iso_->title_id);
            detected = id;
            break;
          }
          case IsoCheck::Result::kNoExecutable:
            error_title = t_.wrong_game_title;
            error_text = Format(t_.wrong_game_text, game::kName);
            detected = t_.detected_no_xex;
            break;
          case IsoCheck::Result::kNotXbox:
            error_title = t_.wrong_game_title;
            error_text = Format(t_.wrong_game_text, game::kName);
            detected = t_.detected_none;
            break;
          default:
            error_title = t_.unreadable_title;
            error_text = t_.unreadable_text;
            detected = t_.detected_none;
            break;
        }
      }
      constexpr Type kErrorTitle{Font::kBold, 16};
      constexpr Type kErrorText{Font::kRegular, 14};
      constexpr Type kDetail{Font::kRegular, 13};
      float error_text_height =
          invalid ? DrawParagraph(kErrorText, ImVec2(0, 0), width - Px(40.0f) - Px(60.0f), 1.5f, 0, error_text, false) : 0;
      float error_head = std::max(Px(44.0f), LineHeight(kErrorTitle) + Px(6.0f) + error_text_height);
      float details = Px(14.0f) * 2.0f + LineHeight(kDetail) * 2.0f + Px(6.0f);
      float error_card = Px(20.0f) * 2.0f + error_head + Px(16.0f) + details + Px(16.0f) + Px(44.0f);
      float body = invalid ? error_card
                           : Px(168.0f) + (checking || iso_ ? Px(14.0f) + card : 0.0f) +
                                 (install_error_.empty() ? 0.0f : Px(12.0f) + LineHeight(kCardHint));
      float height = Px(48.0f) + Px(14.0f) + lead + Px(28.0f) + body;
      float y = centered(height);
      float x = main.Min.x;
      DrawLabel(kHeading, ImVec2(x, y), col::kText, t_.choose_title, Px(48.0f));
      y += Px(48.0f) + Px(14.0f);
      DrawParagraph(kLead, ImVec2(x, y), width, 1.55f, col::kTextSecondary, text);
      y += lead + Px(28.0f);

      if (invalid) {
        ImRect r(ImVec2(x, y), ImVec2(x + width, y + error_card));
        FillRect(r, col::kErrorPanel, 12.0f);
        StrokeRect(r, col::kErrorPanelBorder, 12.0f);
        float cy = r.Min.y + Px(20.0f);
        ImRect box(ImVec2(r.Min.x + Px(20.0f), cy), ImVec2(r.Min.x + Px(64.0f), cy + Px(44.0f)));
        FillRect(box, col::kErrorBadge, 8.0f);
        DrawIcon(Icon::kAlert, ImVec2(box.Min.x + Px(11.0f), box.Min.y + Px(11.0f)), 22.0f, col::kError, 2.0f);
        float tx = box.Max.x + Px(16.0f);
        DrawLabel(kErrorTitle, ImVec2(tx, cy), col::kError, error_title);
        DrawParagraph(kErrorText, ImVec2(tx, cy + LineHeight(kErrorTitle) + Px(6.0f)), r.Max.x - Px(20.0f) - tx, 1.5f,
                      col::kTextSoft, error_text);
        cy += error_head + Px(16.0f);
        ImRect grid(ImVec2(r.Min.x + Px(20.0f), cy), ImVec2(r.Max.x - Px(20.0f), cy + details));
        FillRect(grid, col::kCard, 8.0f);
        float gx = grid.Min.x + Px(16.0f);
        float gy = grid.Min.y + Px(14.0f);
        float value_x = gx + Px(140.0f) + Px(16.0f);
        DrawLabel(kDetail, ImVec2(gx, gy), col::kLabel, t_.file_label);
        DrawLabel(kDetail, ImVec2(value_x, gy), col::kText,
                  Ellipsize(kDetail, Utf8(iso_->path.filename()), grid.Max.x - Px(16.0f) - value_x));
        gy += LineHeight(kDetail) + Px(6.0f);
        DrawLabel(kDetail, ImVec2(gx, gy), col::kLabel, t_.detected_label);
        DrawLabel(kDetail, ImVec2(value_x, gy), col::kText, Ellipsize(kDetail, detected, grid.Max.x - Px(16.0f) - value_x));
        cy += details + Px(16.0f);
        ButtonSpec other{ButtonKind::kLight, Type{Font::kBold, 14}, 44, 18, 8, Icon::kFolder, 16, 2};
        if (Button("other", ImVec2(r.Min.x + Px(20.0f), cy), other, t_.choose_other)) {
          BrowseIso();
        }
        default_focus();
        DrawLabel(Type{Font::kRegular, 13}, ImVec2(r.Min.x + Px(20.0f) + ButtonWidth(other, t_.choose_other) + Px(12.0f), cy),
                  col::kTextMuted, t_.or_drop, Px(44.0f));
      } else {
        // The drop zone, with its button for the file dialog.
        ImRect zone(ImVec2(x, y), ImVec2(x + width, y + Px(168.0f)));
        FillRect(zone, col::kDropZone, 12.0f);
        DashedRect(zone, col::kDashed, 12.0f, 6.0f, 4.0f);
        constexpr Type kDrop{Font::kSemiBold, 15};
        ButtonSpec browse{ButtonKind::kOutline, Type{Font::kSemiBold, 14}, 40, 18, 8};
        float content = Px(32.0f) + Px(10.0f) + LineHeight(kDrop) + Px(10.0f) + Px(40.0f);
        float zy = zone.Min.y + (zone.GetHeight() - content) * 0.5f;
        DrawIcon(Icon::kUpload, ImVec2(zone.GetCenter().x - Px(16.0f), zy), 32.0f, col::kTextDim, 1.6f);
        zy += Px(32.0f) + Px(10.0f);
        DrawLabel(kDrop, ImVec2(zone.GetCenter().x - MeasureText(kDrop, t_.drop_here).x * 0.5f, zy), col::kText,
                  t_.drop_here);
        zy += LineHeight(kDrop) + Px(10.0f);
        float browse_width = ButtonWidth(browse, t_.browse);
        if (Button("browse", ImVec2(zone.GetCenter().x - browse_width * 0.5f, zy), browse, t_.browse)) {
          BrowseIso();
        }
        if (!iso_) {
          default_focus();
        }
        y = zone.Max.y;
        if (checking || iso_) {
          // The file: checking, or valid.
          y += Px(14.0f);
          ImRect r(ImVec2(x, y), ImVec2(x + width, y + card));
          FillRect(r, col::kCard, 10.0f);
          StrokeRect(r, valid ? col::kValidBorder : col::kSidebarBorder, 10.0f);
          ImRect box(ImVec2(r.Min.x + Px(16.0f), r.Min.y + (card - Px(44.0f)) * 0.5f),
                     ImVec2(r.Min.x + Px(60.0f), r.Min.y + (card + Px(44.0f)) * 0.5f));
          FillRect(box, col::kChip, 8.0f);
          DrawIcon(Icon::kDisc, ImVec2(box.Min.x + Px(11.0f), box.Min.y + Px(11.0f)), 22.0f, col::kTextSoft, 1.8f);
          constexpr Type kStatus{Font::kBold, 14};
          const char* status = checking ? t_.checking_iso : t_.iso_valid;
          float status_width = MeasureText(kStatus, status).x + (valid ? Px(22.0f) : 0.0f);
          float tx = box.Max.x + Px(16.0f);
          float text_width = r.Max.x - Px(16.0f) - status_width - Px(16.0f) - tx;
          float block = LineHeight(kCardTitle) + Px(3.0f) + LineHeight(kCardHint) +
                        (valid ? Px(6.0f) + LineHeight(kCardHint) : 0.0f);
          float ty = r.Min.y + (card - block) * 0.5f;
          std::string name = checking ? iso_pending_ : Utf8(iso_->path.filename());
          DrawLabel(kCardTitle, ImVec2(tx, ty), col::kText, Ellipsize(kCardTitle, name, text_width));
          if (!checking) {
            std::string where = Utf8(iso_->path.parent_path()) + " · " + Gigabytes(iso_->file_size) + " GB";
            DrawLabel(kCardHint, ImVec2(tx, ty + LineHeight(kCardTitle) + Px(3.0f)), col::kTextMuted,
                      Ellipsize(kCardHint, where, text_width));
            if (valid) {
              DrawLabel(kCardHint, ImVec2(tx, ty + LineHeight(kCardTitle) + Px(3.0f) + LineHeight(kCardHint) + Px(6.0f)),
                        col::kTextSoft,
                        Ellipsize(kCardHint, Format(t_.iso_detail, game::kName, game::kRegion), text_width));
            }
          }
          float sx = r.Max.x - Px(16.0f) - status_width;
          if (valid) {
            DrawIcon(Icon::kCheck, ImVec2(sx, r.Min.y + (card - Px(16.0f)) * 0.5f), 16.0f, col::kSuccess, 2.6f);
            sx += Px(22.0f);
          }
          DrawLabel(kStatus, ImVec2(sx, r.Min.y), checking ? col::kAccentLight : col::kSuccess, status, card);
          y = r.Max.y;
        }
        if (!install_error_.empty()) {
          DrawLabel(kCardHint, ImVec2(x, y + Px(12.0f)), col::kError, install_error_);
        }
      }

      if (Button("back", ImVec2(main.Min.x, footer_y), FooterButton(ButtonKind::kGhost), t_.back) || back_pressed) {
        SetStep(InstallStep::kWelcome);
      }
      ButtonSpec extract = WithArrow(FooterButton(valid ? ButtonKind::kPrimary : ButtonKind::kDisabled));
      if (!valid) extract.icon = Icon::kNone;
      if (Button("extract", ImVec2(main.Max.x - ButtonWidth(extract, t_.extract), footer_y), extract, t_.extract,
                 valid)) {
        uint64_t free = FreeSpace(exe_dir_);
        uint64_t needed = iso_->image.total_bytes + 64ull * 1024 * 1024;
        if (free && free < needed) {
          install_error_ = Format(t_.no_space, Gigabytes(needed), Gigabytes(free));
        } else {
          install_error_.clear();
          rate_ = 0.0;
          rate_time_ = {};
          extraction_.Start(iso_->path, iso_->image, exe_dir_ / "assets");
          SetStep(InstallStep::kExtracting);
        }
      }
      if (valid) {
        default_focus();
      }
      break;
    }

    case InstallStep::kExtracting: {
      Extraction::Progress progress = extraction_.Get();
      bool done = progress.state == Extraction::State::kDone;
      bool failed = progress.state == Extraction::State::kFailed;
      float width = std::min(main.GetWidth(), Px(640.0f));
      const char* title = done ? t_.extracted_title : failed ? t_.extract_failed_title : t_.extracting_title;
      std::string text = done     ? std::string(t_.extracted_text)
                         : failed ? Format(t_.extract_failed_text, progress.error_path, progress.error)
                                  : std::string(t_.extracting_text);
      float lead = DrawParagraph(kLead, ImVec2(0, 0), width, 1.55f, 0, text, false);
      constexpr Type kPercent{Font::kCondensedBold, 64};
      constexpr Type kLabelType{Font::kBold, 12, 0.1f};
      constexpr Type kFile{Font::kRegular, 14};
      float card = Px(28.0f) * 2.0f + Px(64.0f) + Px(16.0f) + Px(12.0f) + Px(16.0f) + LineHeight(kLabelType) + Px(4.0f) +
                   LineHeight(kFile);
      float height = Px(48.0f) + Px(14.0f) + lead + Px(32.0f) + card + Px(16.0f) + LineHeight(kCardHint);
      float y = centered(height);
      float x = main.Min.x;
      DrawLabel(kHeading, ImVec2(x, y), failed ? col::kError : col::kText, title, Px(48.0f));
      y += Px(48.0f) + Px(14.0f);
      DrawParagraph(kLead, ImVec2(x, y), width, 1.55f, col::kTextSecondary, text);
      y += lead + Px(32.0f);

      ImRect r(ImVec2(x, y), ImVec2(x + width, y + card));
      FillRect(r, col::kCard, 12.0f);
      float fraction = progress.total ? float(double(progress.done) / double(progress.total)) : 0.0f;
      int percent = done ? 100 : std::min(99, int(fraction * 100.0f));
      char percent_text[16];
      std::snprintf(percent_text, sizeof(percent_text), "%d%%", percent);
      float cy = r.Min.y + Px(28.0f);
      DrawLabel(kPercent, ImVec2(r.Min.x + Px(28.0f), cy), col::kAccent, percent_text, Px(64.0f));
      std::string left = "—";
      if (done) {
        left = "00:00";
      } else if (rate_ > 1.0 && progress.total > progress.done) {
        int seconds = int(double(progress.total - progress.done) / rate_ + 0.5);
        char mmss[16];
        std::snprintf(mmss, sizeof(mmss), "%02d:%02d", seconds / 60, seconds % 60);
        left = mmss;
      }
      std::string time_left = Format(t_.time_left, left);
      constexpr Type kTime{Font::kRegular, 14};
      // On the percentage's baseline.
      DrawLabel(kTime, ImVec2(r.Max.x - Px(28.0f) - MeasureText(kTime, time_left).x, cy + Px(64.0f) - LineHeight(kTime)),
                col::kTextMuted, time_left);
      cy += Px(64.0f) + Px(16.0f);
      Progress(ImRect(ImVec2(r.Min.x + Px(28.0f), cy), ImVec2(r.Max.x - Px(28.0f), cy + Px(12.0f))),
               done ? 1.0f : fraction, col::kTrack, failed ? col::kError : col::kAccent);
      cy += Px(12.0f) + Px(16.0f);
      DrawLabel(kLabelType, ImVec2(r.Min.x + Px(28.0f), cy), col::kLabel, t_.current_file);
      cy += LineHeight(kLabelType) + Px(4.0f);
      DrawLabel(kFile, ImVec2(r.Min.x + Px(28.0f), cy), col::kTextSoft,
                Ellipsize(kFile, done || progress.current.empty() ? "—" : progress.current, width - Px(56.0f)));
      y = r.Max.y + Px(16.0f);
      DrawIcon(Icon::kCheck, ImVec2(x, y + (LineHeight(kCardHint) - Px(16.0f)) * 0.5f), 16.0f, col::kSuccess, 2.4f);
      DrawLabel(kCardHint, ImVec2(x + Px(24.0f), y), col::kTextMuted, Format(t_.iso_verified, game::kName));

      if (failed) {
        if (Button("back", ImVec2(main.Min.x, footer_y), FooterButton(ButtonKind::kGhost), t_.back) || back_pressed) {
          SetStep(InstallStep::kChooseIso);
        }
      } else if (!done) {
        if (Button("cancel", ImVec2(main.Min.x, footer_y), FooterButton(ButtonKind::kGhost), t_.cancel_install)) {
          extraction_.Cancel();
          SetStep(InstallStep::kChooseIso);
        }
      }
      ButtonSpec next = WithArrow(FooterButton(done ? ButtonKind::kPrimary : ButtonKind::kDisabled));
      if (!done) next.icon = Icon::kNone;
      if (Button("continue", ImVec2(main.Max.x - ButtonWidth(next, t_.continue_label), footer_y), next,
                 t_.continue_label, done)) {
        SetStep(InstallStep::kDone);
      }
      if (done) {
        default_focus();
        if (!achievements_requested_) {
          LoadAchievements();  // the game can write their list now
        }
      }
      break;
    }

    case InstallStep::kDone: {
      float width = std::min(main.GetWidth(), Px(600.0f));
      // The game's own (game.cpp), or the launcher's.
      const game::GameTexts& own = game::GameTextsFor(language_);
      const char* done_title = own.done_title ? own.done_title : t_.done_title;
      const char* done_text = own.done_text ? own.done_text : t_.done_text;
      float lead = DrawParagraph(kLead, ImVec2(0, 0), width, 1.55f, 0, done_text, false);
      constexpr Type kNote{Font::kRegular, 13};
      float note = Px(14.0f) * 2.0f + DrawParagraph(kNote, ImVec2(0, 0), width - Px(32.0f), 1.5f, 0, t_.delete_iso, false);
      float height = Px(72.0f) + Px(28.0f) + Px(48.0f) + Px(14.0f) + lead + Px(24.0f) + note;
      float y = centered(height);
      float x = main.Min.x;
      ImRect tile(ImVec2(x, y), ImVec2(x + Px(72.0f), y + Px(72.0f)));
      FillRect(tile, col::kDoneTile, 16.0f);
      DrawIcon(Icon::kCheck, ImVec2(tile.Min.x + Px(18.0f), tile.Min.y + Px(18.0f)), 36.0f, col::kSuccess, 2.4f);
      y += Px(72.0f) + Px(28.0f);
      DrawLabel(kHeading, ImVec2(x, y), col::kText, done_title, Px(48.0f));
      y += Px(48.0f) + Px(14.0f);
      DrawParagraph(kLead, ImVec2(x, y), width, 1.55f, col::kTextSecondary, done_text);
      y += lead + Px(24.0f);
      ImRect box(ImVec2(x, y), ImVec2(x + width, y + note));
      FillRect(box, col::kCard, 10.0f);
      DrawParagraph(kNote, ImVec2(x + Px(16.0f), y + Px(14.0f)), width - Px(32.0f), 1.5f, col::kTextMuted, t_.delete_iso);

      ButtonSpec play{ButtonKind::kPrimary, Type{Font::kCondensedBold, 24, 0.04f}, 52, 36, 10, Icon::kPlay, 20, 0, true};
      play.gap = 12;
      float play_width = ButtonWidth(play, t_.play);
      ButtonSpec review = FooterButton(ButtonKind::kGhost);
      float review_x = main.Max.x - play_width - Px(12.0f) - ButtonWidth(review, t_.review_settings);
      if (Button("review", ImVec2(review_x, footer_y), review, t_.review_settings)) {
        installing_ = false;
        Navigate(Page::kVideo, false);
      }
      if (Button("play", ImVec2(main.Max.x - play_width, footer_y), play, t_.play)) {
        result_ = Result::kPlay;
      }
      default_focus();
      break;
    }
  }
}

}  // namespace launcher
