// The Play page: the art with the game's name, PLAY and Quit, and cards for the current settings, the
// achievements and the updates.

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "app.h"

namespace launcher {

using namespace ui;

namespace {

constexpr Type kCardTitle{Font::kBold, 12, 0.1f};
constexpr Type kChip{Font::kMedium, 13};
constexpr Type kLinkType{Font::kSemiBold, 14};

std::string SizeText(Size size) { return std::to_string(size.first) + " × " + std::to_string(size.second); }

}  // namespace

void App::PlayPage(const ImRect& rect) {
  const Options& o = settings_.o;
  ImRect content(ImVec2(rect.Min.x + Px(24.0f), rect.Min.y + Px(24.0f)),
                 ImVec2(rect.Max.x - Px(24.0f), rect.Max.y - Px(24.0f)));
  float cards_height = Px(172.0f);
  ImRect hero(content.Min, ImVec2(content.Max.x, content.Max.y - cards_height - Px(16.0f)));

  // The art and the game's name.
  FillRect(hero, Hex(0x142033), 12.0f);
  // Cropped around the art's focus (game.h): a wide window keeps it whole.
  DrawArt(Art::kHero, hero, ImVec2(game::kHeroFocusX, game::kHeroFocusY), 12.0f);
  float left = hero.Min.x + Px(40.0f);
  float right = hero.Max.x - Px(40.0f);
  float bottom = hero.Max.y - Px(36.0f);
  // A notice across the top of the art: the game closed with an error, or the launcher was just updated.
  ImRect notice(ImVec2(hero.Min.x + Px(24.0f), hero.Min.y + Px(24.0f)),
                ImVec2(hero.Max.x - Px(24.0f), hero.Min.y + Px(24.0f) + Px(52.0f)));
  if (game_exit_code_ != 0) {
    // A crash's code (0xC0000005) in hexadecimal, a small one as it is.
    char code[16];
    std::snprintf(code, sizeof(code), game_exit_code_ < 0x10000 ? "%u" : "0x%08X", game_exit_code_);
    char text[256];
    std::snprintf(text, sizeof(text), t_.game_closed_error, code);
    Notice(notice, true, text, t_.open_logs, Page::kCount);
  } else if (!updated_from_.empty()) {
    char text[128];
    std::snprintf(text, sizeof(text), t_.updated_to, kVersion);
    Notice(notice, false, text, t_.see_whats_new, Page::kUpdates);
  }

  ButtonSpec play{ButtonKind::kPrimary, Type{Font::kCondensedBold, 26, 0.04f}, 60, 40, 10, Icon::kPlay, 22, 0, true};
  play.gap = 12;
  ButtonSpec quit{ButtonKind::kHero, Type{Font::kSemiBold, 16}, 60, 24, 10, Icon::kLogOut, 20, 1.8f};
  quit.gap = 10;
  float play_width = ButtonWidth(play, t_.play);
  float quit_width = ButtonWidth(quit, t_.quit);
  float buttons_y = bottom - Px(60.0f);
  float play_x = right - play_width;
  if (Button("quit", ImVec2(play_x - Px(12.0f) - quit_width, buttons_y), quit, t_.quit)) {
    result_ = Result::kQuit;
  }
  if (Button("play", ImVec2(play_x, buttons_y), play, t_.play)) {
    result_ = Result::kPlay;
  }
  // The window's first focus (ImGui's navigation picks it when the window appears): Enter or A plays straight
  // away, as in the launcher before (its default button).
  ImGui::SetItemDefaultFocus();
  page_focus_ = ImGui::GetItemID();

  constexpr Type kKicker{Font::kBold, 13, 0.16f};
  constexpr Type kName{Font::kCondensedBold, 76, -0.01f};
  constexpr Type kTagline{Font::kRegular, 15};
  float tagline_y = bottom - LineHeight(kTagline);
  float name_box = Px(76.0f) * 0.92f;
  float name_y = tagline_y - Px(12.0f) - name_box;
  float kicker_y = name_y - Px(6.0f) - LineHeight(kKicker);
  DrawLabel(kKicker, ImVec2(left, kicker_y), col::kAccent, game::kArtKicker);
  DrawLabel(kName, ImVec2(left, name_y), col::kText, game::kArtTitle, name_box);
  DrawLabel(kTagline, ImVec2(left, tagline_y), col::kTextSecondary, t_.tagline);

  // The cards.
  float gap = Px(16.0f);
  float card_width = (content.GetWidth() - gap * 2.0f) / 3.0f;
  float card_top = content.Max.y - cards_height;
  auto card = [&](int i) {
    float x = content.Min.x + float(i) * (card_width + gap);
    return ImRect(ImVec2(x, card_top), ImVec2(x + card_width, content.Max.y));
  };
  float link_y = content.Max.y - Px(20.0f) - LineHeight(kLinkType);

  // The current settings.
  {
    ImRect r = card(0);
    FillRect(r, col::kCard, 10.0f);
    ImVec2 p(r.Min.x + Px(20.0f), r.Min.y + Px(20.0f));
    DrawLabel(kCardTitle, p, col::kLabel, t_.current_settings);
    std::vector<std::string> chips;
    chips.push_back(SizeText(settings_.OutputSize()));
    chips.push_back(o.display == kFullscreen ? t_.fullscreen : o.display == kBorderless ? t_.borderless : t_.window);
    if (game::kHasNativeXRenderer) {
      chips.push_back(o.native ? "NativeXRenderer" : "Xenos");
    }
    if (*game::kFrameRateCvar) {
      for (const game::FrameRate& rate : game::kFrameRates) {
        if (rate.value == o.frame_rate) chips.push_back(std::to_string(rate.fps) + " FPS");
      }
    }
    if (o.native) {
      if (o.native_aa == kAaOff) {
        chips.push_back(std::string(t_.antialiasing) + ": " + t_.none);
      } else if (o.native_aa < kNativeAntialiasingCount) {
        constexpr const char* kNames[] = {"SMAA", "DLAA", "FXAA", ""};
        chips.push_back(kNames[o.native_aa]);
      } else if (o.native_aa_experimental >= 0) {
        chips.push_back(kExperimentalAntialiasing[o.native_aa_experimental].label);
      }
    } else {
      if (*game::kNoMsaaCvar) {
        chips.push_back(o.no_msaa ? std::string(t_.antialiasing) + ": " + t_.none : std::string(game::kMsaaLabel));
      }
    }
    constexpr const char* kAnisotropic[] = {"", "", "2×", "4×", "8×", "16×"};
    chips.push_back(o.anisotropic > 0 ? std::string("AF ") + kAnisotropic[o.anisotropic] : std::string("AF ") + t_.off);
    // The chips wrap; a third row pushes the links down into the cards' bottom padding, as in the mockup
    // (its flex column overflows the same way). What still does not fit is left out.
    std::vector<ImRect> places;
    float x = p.x;
    float y = p.y + LineHeight(kCardTitle) + Px(14.0f);
    float chip_height = LineHeight(kChip) + Px(10.0f);
    for (const std::string& chip : chips) {
      float chip_width = MeasureText(kChip, chip).x + Px(20.0f);
      if (x > p.x && x + chip_width > r.Max.x - Px(20.0f)) {
        x = p.x;
        y += chip_height + Px(6.0f);
      }
      places.emplace_back(ImVec2(x, y), ImVec2(x + chip_width, y + chip_height));
      x += chip_width + Px(6.0f);
    }
    float lowest_link = r.Max.y - LineHeight(kLinkType) - Px(4.0f);
    link_y = std::clamp(places.back().Max.y + Px(14.0f), link_y, std::max(link_y, lowest_link));
    for (size_t i = 0; i < places.size() && places[i].Max.y <= link_y - Px(6.0f); ++i) {
      FillRect(places[i], col::kChip, 6.0f);
      DrawLabel(kChip, ImVec2(places[i].Min.x + Px(10.0f), places[i].Min.y + Px(5.0f)), col::kText, chips[i]);
    }
    if (Link("settings", ImVec2(p.x, link_y), t_.change_settings)) {
      Navigate(Page::kVideo, false);
    }
  }

  // The achievements (page_achievements.cpp).
  AchievementsCard(card(1), link_y);

  // The updates.
  {
    ImRect r = card(2);
    FillRect(r, col::kCard, 10.0f);
    ImVec2 p(r.Min.x + Px(20.0f), r.Min.y + Px(20.0f));
    DrawLabel(kCardTitle, p, col::kLabel, t_.updates_card);
    constexpr Type kVersionType{Font::kSemiBold, 16};
    float y = p.y + LineHeight(kCardTitle) + Px(12.0f);
    float line = LineHeight(kVersionType);
    // The installed version with a green dot, or the new one with an accent dot.
    const Release* newer = updates_ ? updates_->Newer(Version::Parse(kVersion)) : nullptr;
    FillRect(ImRect(ImVec2(p.x, y + (line - Px(10.0f)) * 0.5f), ImVec2(p.x + Px(10.0f), y + (line + Px(10.0f)) * 0.5f)),
             newer ? col::kAccent : col::kSuccess, 5.0f);
    char version[96];
    if (newer) {
      std::string number = newer->tag.size() > 1 && (newer->tag[0] == 'v' || newer->tag[0] == 'V') ? newer->tag.substr(1)
                                                                                                    : newer->tag;
      std::snprintf(version, sizeof(version), t_.update_card_available, number.c_str());
    } else {
      std::snprintf(version, sizeof(version), t_.version, kVersion);
    }
    DrawLabel(kVersionType, ImVec2(p.x + Px(20.0f), y), col::kText,
              Ellipsize(kVersionType, version, r.GetWidth() - Px(60.0f)));
    y += line + Px(12.0f);
    char checked[128];
    if (update_url_.empty()) {
      std::snprintf(checked, sizeof(checked), "%s", t_.updates_off);
    } else {
      std::snprintf(checked, sizeof(checked), t_.last_check, LastCheckedText().c_str());
    }
    DrawLabel(Type{Font::kRegular, 13}, ImVec2(p.x, y), col::kTextMuted,
              Ellipsize(Type{Font::kRegular, 13}, checked, r.GetWidth() - Px(40.0f)));
    if (!update_url_.empty() && Link("updates", ImVec2(p.x, link_y), newer ? t_.update_now : t_.find_updates)) {
      Navigate(Page::kUpdates, false);
    }
  }
}

void App::Notice(const ImRect& rect, bool error, const char* text, const char* link, Page link_page) {
  ImU32 color = error ? col::kError : col::kSuccess;
  FillRect(rect, error ? col::kErrorTint : Hex(0x17281F), 10.0f);
  StrokeRect(rect, Fade(color, 0.55f), 10.0f);
  ImVec2 icon(rect.Min.x + Px(16.0f), rect.Min.y + (rect.GetHeight() - Px(20.0f)) * 0.5f);
  DrawIcon(error ? Icon::kAlert : Icon::kCheck, icon, 20.0f, color, 2.0f);
  constexpr Type kText{Font::kMedium, 14};
  float x = rect.Min.x + Px(48.0f);
  DrawLabel(kText, ImVec2(x, rect.Min.y), col::kText, text, rect.GetHeight());
  constexpr Type kLinkType{Font::kSemiBold, 14};
  float link_width = MeasureText(kLinkType, link).x + Px(20.0f);
  float link_x = std::max(x + MeasureText(kText, text).x + Px(24.0f), rect.Max.x - Px(16.0f) - link_width);
  if (Link("notice", ImVec2(link_x, rect.Min.y + (rect.GetHeight() - LineHeight(kLinkType)) * 0.5f), link)) {
    if (link_page == Page::kCount) {
      OpenFolder(exe_dir_ / L"logs");
    } else {
      Navigate(link_page, false);
    }
  }
}

}  // namespace launcher
