// The Game page, in the Settings group: the game's own options (game.h), its language and its switches. Only for a
// game that has them (game::kHasGamePage). Every change is saved at once.

#include <algorithm>
#include <string>
#include <vector>

#include "app.h"
#include "pages.h"

namespace launcher {

using namespace ui;

void App::GamePage(const ImRect& rect) {
  Options& o = settings_.o;
  ImRect content = PageContent(rect);
  float y = Header(content.Min, t_.settings, t_.nav_game, t_.game_subtitle) + Px(20.0f);
  float width = content.GetWidth();
  ImVec2 at;
  bool changed = false;
  const game::GameTexts& own = game::GameTextsFor(language_);
  const size_t rows = (game::kLanguages.empty() ? 0 : 1) + game::kToggles.size();
  size_t row = 0;
  Card card;
  float top = y;

  // The language, each in its own words.
  if (!game::kLanguages.empty()) {
    std::vector<std::string> items;
    for (const game::Language& l : game::kLanguages) {
      items.push_back(l.name);
    }
    int selected = std::clamp(o.language, 0, int(items.size()) - 1);
    float combo_width = std::min(Px(420.0f), width - Px(28.0f + 300.0f + 24.0f + 28.0f));
    y += Row(ImVec2(content.Min.x, y), width, 20, t_.game_language, t_.game_language_hint, 44, ++row == rows, &at);
    if (Combo("language", ImRect(at, ImVec2(at.x + combo_width, at.y + Px(44.0f))), items, &selected)) {
      o.language = selected;
      changed = true;
    }
  }

  // The switches, with game.cpp's titles and hints.
  for (size_t i = 0; i < game::kToggles.size(); ++i) {
    y += Row(ImVec2(content.Min.x, y), width, 20, own.toggles[i * 2], own.toggles[i * 2 + 1], 30, ++row == rows, &at);
    ImGui::PushID(int(i));
    if (Toggle("toggle", at, &o.toggles[i])) {
      changed = true;
    }
    ImGui::PopID();
  }
  card.End(ImRect(ImVec2(content.Min.x, top), ImVec2(content.Max.x, y)));

  if (Footer(content, t_.reset_values)) {
    ResetGame(o);
    changed = true;
  }
  if (changed) {
    Changed();
  }
}

}  // namespace launcher
