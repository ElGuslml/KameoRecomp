// The Achievements page and the Play page's achievements card, from the list the game writes for launchers
// (achievements.h): the totals, the progress, filters, and every achievement with the game's own icon (dimmed with
// a lock while locked), its gamerscore and its unlock date.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "app.h"
#include "pages.h"

namespace launcher {

using namespace ui;

namespace {

// An achievement's icon in `box`: the game's own, dimmed with a lock while locked; a question mark for a secret
// one still locked.
void DrawAchievementIcon(ImTextureID texture, const ImRect& box, bool unlocked, bool secret) {
  ImDrawList* draw = ImGui::GetWindowDrawList();
  if (secret || texture == ImTextureID_Invalid) {
    FillRect(box, col::kGlyph, 8.0f);
    constexpr Type kMark{Font::kCondensedBold, 26};
    const char* mark = secret ? "?" : "G";
    DrawLabel(kMark, ImVec2(box.GetCenter().x - MeasureText(kMark, mark).x * 0.5f, box.Min.y), col::kTextDim, mark,
              box.GetHeight());
  } else {
    draw->AddImageRounded(texture, box.Min, box.Max, ImVec2(0, 0), ImVec2(1, 1),
                          unlocked ? IM_COL32_WHITE : IM_COL32(92, 96, 104, 255), Px(8.0f));
  }
  StrokeRect(box, Fade(col::kText, 0.08f), 8.0f);
  if (!unlocked && !secret) {
    // The lock, in a badge on the corner.
    float badge = Px(20.0f);
    ImVec2 center(box.Max.x - badge * 0.5f + Px(3.0f), box.Max.y - badge * 0.5f + Px(3.0f));
    draw->AddCircleFilled(center, badge * 0.5f, col::kCard);
    draw->AddCircleFilled(center, badge * 0.5f - Px(1.5f), col::kChip);
    DrawIcon(Icon::kLock, ImVec2(center.x - Px(6.0f), center.y - Px(6.0f)), 12.0f, col::kTextMuted, 2.0f);
  }
}

// A big number and the rest of its line ("7" "/ 30") on one baseline, from `x`; returns where it ends.
float NumberPair(ImVec2 pos, const Type& big, const Type& small_type, const std::string& value, const std::string& rest,
                 ImU32 color) {
  DrawLabel(big, pos, color, value);
  float width = MeasureText(big, value).x;
  DrawLabel(small_type,
            ImVec2(pos.x + width + Px(6.0f), pos.y + LineHeight(big) - LineHeight(small_type) - Px(2.0f)),
            col::kTextMuted, rest);
  return pos.x + width + Px(6.0f) + MeasureText(small_type, rest).x;
}

}  // namespace

void App::AchievementsPage(const ImRect& rect) {
  ImRect content = PageContent(rect);
  content.Max.x = rect.Max.x - Px(40.0f);  // the list uses the whole width
  float header_bottom = Header(content.Min, nullptr, t_.achievements_title, t_.achievements_subtitle);
  float y = header_bottom + Px(20.0f);

  if (!achievements_ || achievements_->items.empty()) {
    // Nothing to list before the game has written the list (it is being asked to, or could not).
    constexpr Type kTitle{Font::kSemiBold, 18};
    constexpr Type kText{Font::kRegular, 14};
    float text_width = std::min(Px(520.0f), content.GetWidth() - Px(80.0f));
    float text_height = DrawParagraph(kText, ImVec2(0, 0), text_width, 1.5f, 0, t_.achievements_empty, false);
    float height = Px(48.0f) * 2.0f + Px(64.0f) + Px(20.0f) + LineHeight(kTitle) + Px(8.0f) + text_height;
    ImRect card(ImVec2(content.Min.x, y), ImVec2(content.Max.x, y + height));
    FillRect(card, col::kCard, 12.0f);
    float cx = card.GetCenter().x;
    float top = card.Min.y + Px(48.0f);
    ImRect icon(ImVec2(cx - Px(32.0f), top), ImVec2(cx + Px(32.0f), top + Px(64.0f)));
    FillRect(icon, col::kAccentTint, 12.0f);
    DrawIcon(Icon::kTrophy, ImVec2(icon.Min.x + Px(17.0f), icon.Min.y + Px(17.0f)), 30.0f, col::kAccent, 1.8f);
    top += Px(64.0f) + Px(20.0f);
    DrawLabel(kTitle, ImVec2(cx - MeasureText(kTitle, t_.achievements_empty_title).x * 0.5f, top), col::kText,
              t_.achievements_empty_title);
    top += LineHeight(kTitle) + Px(8.0f);
    DrawParagraph(kText, ImVec2(cx - text_width * 0.5f, top), text_width, 1.5f, col::kTextMuted, t_.achievements_empty,
                  true, 0.5f);
    return;
  }
  const Achievements& list = *achievements_;
  int total = int(list.items.size());

  // The filters, right of the title.
  {
    char all[64], unlocked[64], locked[64];
    std::snprintf(all, sizeof(all), t_.filter_all, total);
    std::snprintf(unlocked, sizeof(unlocked), t_.filter_unlocked, list.unlocked);
    std::snprintf(locked, sizeof(locked), t_.filter_locked, total - list.unlocked);
    std::string labels[] = {all, unlocked, locked};
    SegmentedSpec spec{40, 16, 0, col::kCard};
    ImVec2 size = SegmentedSize(spec, labels);
    float top = content.Min.y + std::max(0.0f, (header_bottom - content.Min.y - size.y) * 0.5f);
    Segmented("filter", ImVec2(content.Max.x - size.x, top), spec, labels, &achievement_filter_);
  }

  // The totals and the progress.
  {
    float height = Px(94.0f);
    ImRect card(ImVec2(content.Min.x, y), ImVec2(content.Max.x, y + height));
    FillRect(card, col::kCard, 12.0f);
    constexpr Type kLabelType{Font::kBold, 12, 0.1f};
    constexpr Type kBig{Font::kCondensedBold, 28};
    constexpr Type kSmall{Font::kCondensedBold, 20};
    float x = card.Min.x + Px(24.0f);
    float label_y = card.Min.y + Px(20.0f);
    float value_y = label_y + LineHeight(kLabelType) + Px(4.0f);
    DrawLabel(kLabelType, ImVec2(x, label_y), col::kLabel, t_.unlocked_total);
    float end = NumberPair(ImVec2(x, value_y), kBig, kSmall, std::to_string(list.unlocked), "/ " + std::to_string(total),
                           col::kText);
    float x2 = std::max(end, x + MeasureText(kLabelType, t_.unlocked_total).x) + Px(40.0f);
    DrawLabel(kLabelType, ImVec2(x2, label_y), col::kLabel, t_.gamerscore);
    end = NumberPair(ImVec2(x2, value_y), kBig, kSmall, std::to_string(list.gamerscore),
                     "/ " + std::to_string(list.total_gamerscore) + " G", col::kAccent);
    float x3 = std::max(end, x2 + MeasureText(kLabelType, t_.gamerscore).x) + Px(40.0f);
    float right = card.Max.x - Px(24.0f);
    if (right - x3 > Px(120.0f)) {
      constexpr Type kProgress{Font::kRegular, 13};
      int percent = total ? int(std::lround(double(list.unlocked) * 100.0 / double(total))) : 0;
      std::string percent_text = std::to_string(percent) + " %";
      float text_y = card.Min.y + Px(26.0f);
      DrawLabel(kProgress, ImVec2(x3, text_y), col::kTextMuted, t_.progress);
      DrawLabel(kProgress, ImVec2(right - MeasureText(kProgress, percent_text).x, text_y), col::kTextMuted,
                percent_text);
      float bar_y = text_y + LineHeight(kProgress) + Px(10.0f);
      Progress(ImRect(ImVec2(x3, bar_y), ImVec2(right, bar_y + Px(10.0f))),
               total ? float(list.unlocked) / float(total) : 0.0f);
    }
    y += height + Px(20.0f);
  }

  // The list: columns of cards, two at the mockup's size and more on wide windows.
  std::vector<size_t> shown;
  for (size_t i = 0; i < list.items.size(); ++i) {
    bool unlocked = list.items[i].unlocked != 0;
    if (achievement_filter_ == 0 || (achievement_filter_ == 1) == unlocked) {
      shown.push_back(i);
    }
  }
  ImRect area(ImVec2(content.Min.x, y), ImVec2(content.Max.x, content.Max.y));
  if (shown.empty()) {
    constexpr Type kText{Font::kRegular, 15};
    DrawLabel(kText,
              ImVec2(area.GetCenter().x - MeasureText(kText, t_.achievements_filter_empty).x * 0.5f,
                     area.Min.y + Px(40.0f)),
              col::kTextMuted, t_.achievements_filter_empty);
    return;
  }
  BeginScroll("achievements", area);
  {
    ImVec2 origin = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x - Px(16.0f);  // room for the scroll bar
    float gap_x = Px(12.0f);
    float gap_y = Px(10.0f);
    size_t columns = std::clamp(size_t((width + gap_x) / (Px(440.0f) + gap_x)), size_t(2), size_t(4));
    float card_width = std::floor((width - gap_x * float(columns - 1)) / float(columns));
    float card_height = Px(84.0f);
    constexpr Type kName{Font::kSemiBold, 15};
    constexpr Type kText{Font::kRegular, 13};
    constexpr Type kScore{Font::kCondensedBold, 20};
    constexpr Type kState{Font::kSemiBold, 12};
    constexpr float kTextLine = 1.35f;
    for (size_t n = 0; n < shown.size(); ++n) {
      size_t index = shown[n];
      const Achievement& a = list.items[index];
      bool unlocked = a.unlocked != 0;
      bool hidden = !unlocked && a.secret();
      float x = origin.x + float(n % columns) * (card_width + gap_x);
      float top = origin.y + float(n / columns) * (card_height + gap_y);
      ImRect card(ImVec2(x, top), ImVec2(x + card_width, top + card_height));
      // Focusable, so that a controller moves through the list (the region scrolls to it).
      ImGui::PushID(int(a.id));
      ItemState state = Item("card", card);
      ImGui::PopID();
      FillRect(card, state.hovered ? col::kCardRaised : col::kCard, 10.0f);

      ImRect icon(ImVec2(card.Min.x + Px(14.0f), card.Min.y + Px(14.0f)),
                  ImVec2(card.Min.x + Px(70.0f), card.Min.y + Px(70.0f)));
      DrawAchievementIcon(AchievementIcon(index), icon, unlocked, hidden);

      // The gamerscore and the state (the unlock date, or "Locked"), right.
      std::string score = std::to_string(a.gamerscore) + " G";
      std::string state_text = t_.locked;
      if (unlocked) {
        state_text = FormatDate(a.unlocked, language_);
        if (state_text.empty()) {
          state_text = t_.unlocked;
        }
      }
      float right = card.Max.x - Px(14.0f);
      float block = LineHeight(kScore) + Px(2.0f) + LineHeight(kState);
      float score_y = card.Min.y + (card_height - block) * 0.5f;
      DrawLabel(kScore, ImVec2(right - MeasureText(kScore, score).x, score_y),
                unlocked ? col::kAccent : col::kTextSecondary, score);
      float state_y = score_y + LineHeight(kScore) + Px(2.0f);
      float state_width = MeasureText(kState, state_text).x;
      if (unlocked) {
        DrawLabel(kState, ImVec2(right - state_width, state_y), col::kSuccess, state_text);
        DrawIcon(Icon::kCheck,
                 ImVec2(right - state_width - Px(16.0f), state_y + (LineHeight(kState) - Px(12.0f)) * 0.5f), 12.0f,
                 col::kSuccess, 2.4f);
      } else {
        DrawLabel(kState, ImVec2(right - state_width, state_y), col::kLabel, state_text);
      }
      float column = std::max({MeasureText(kScore, score).x, state_width + Px(16.0f), Px(64.0f)});

      // The name and description, between the icon and the column; at most two lines of description.
      float text_x = icon.Max.x + Px(14.0f);
      float text_width = right - column - Px(12.0f) - text_x;
      const char* name = hidden ? t_.achievement_secret : a.label.c_str();
      const char* description =
          hidden ? t_.achievement_secret_hint : (unlocked ? a.description.c_str() : a.unachieved_description.c_str());
      float text_height = std::min(DrawParagraph(kText, ImVec2(0, 0), text_width, kTextLine, 0, description, false),
                                   LineHeight(kText) * kTextLine * 2.0f);
      float block_height = LineHeight(kName) + Px(3.0f) + text_height;
      float text_y = card.Min.y + std::max(Px(12.0f), (card_height - block_height) * 0.5f);
      DrawLabel(kName, ImVec2(text_x, text_y), col::kText, Ellipsize(kName, name, text_width));
      float description_y = text_y + LineHeight(kName) + Px(3.0f);
      ImGui::PushClipRect(ImVec2(text_x, description_y), ImVec2(text_x + text_width, description_y + text_height),
                          true);
      DrawParagraph(kText, ImVec2(text_x, description_y), text_width, kTextLine, col::kTextMuted, description);
      ImGui::PopClipRect();
      if (state.focused) {
        FocusRing(card, 10.0f);
      }
    }
    size_t rows = (shown.size() + columns - 1) / columns;
    ImGui::Dummy(ImVec2(width, float(rows) * (card_height + gap_y)));
  }
  EndScroll();
}

void App::AchievementsCard(const ImRect& r, float link_y) {
  constexpr Type kCardTitle{Font::kBold, 12, 0.1f};
  FillRect(r, col::kCard, 10.0f);
  ImVec2 p(r.Min.x + Px(20.0f), r.Min.y + Px(20.0f));
  DrawLabel(kCardTitle, p, col::kLabel, t_.achievements_card);
  float y = p.y + LineHeight(kCardTitle) + Px(12.0f);
  if (!achievements_ || achievements_->items.empty()) {
    // Before the game has written the list.
    DrawParagraph(Type{Font::kRegular, 14}, ImVec2(p.x, y), r.GetWidth() - Px(40.0f), 1.4f, col::kTextMuted,
                  t_.achievements_after_play);
  } else {
    // "7 / 30", the gamerscore right, and the progress.
    const Achievements& list = *achievements_;
    constexpr Type kBig{Font::kCondensedBold, 28};
    constexpr Type kSmall{Font::kCondensedBold, 20};
    constexpr Type kScore{Font::kCondensedBold, 15};
    int total = int(list.items.size());
    NumberPair(ImVec2(p.x, y), kBig, kSmall, std::to_string(list.unlocked), "/ " + std::to_string(total), col::kText);
    std::string score = std::to_string(list.gamerscore) + " / " + std::to_string(list.total_gamerscore) + " G";
    DrawLabel(kScore,
              ImVec2(r.Max.x - Px(20.0f) - MeasureText(kScore, score).x,
                     y + LineHeight(kBig) - LineHeight(kScore) - Px(3.0f)),
              col::kAccent, score);
    float bar_y = y + LineHeight(kBig) + Px(10.0f);
    Progress(ImRect(ImVec2(p.x, bar_y), ImVec2(r.Max.x - Px(20.0f), bar_y + Px(8.0f))),
             total ? float(list.unlocked) / float(total) : 0.0f);
  }
  if (Link("achievements", ImVec2(p.x, link_y), t_.see_achievements)) {
    Navigate(Page::kAchievements, false);
  }
}

}  // namespace launcher
