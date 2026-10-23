// The Updates page: the installed version and the last check, a new version and its installation, checking when the
// launcher opens, and the release notes (Markdown from GitHub, drawn simply). The updater is updater.h.

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <string>
#include <string_view>
#include <vector>

#include "app.h"
#include "game.h"
#include "pages.h"

namespace launcher {

using namespace ui;

namespace {

// Markdown's inline marks taken out: **bold**, *italic*, __bold__, `code`, [text](url) -> text, ![image](url) ->
// nothing. A single "_" stays (file names have them).
std::string PlainInline(std::string_view text) {
  std::string out;
  for (size_t i = 0; i < text.size();) {
    char c = text[i];
    if (c == '_' && i + 1 < text.size() && text[i + 1] == '_') {
      i += 2;
      continue;
    }
    if (c == '*') {
      ++i;
      continue;
    }
    if (c == '`') {
      ++i;
      continue;
    }
    if (c == '!' && i + 1 < text.size() && text[i + 1] == '[') {
      size_t close = text.find("](", i);
      size_t end = close == std::string_view::npos ? close : text.find(')', close);
      if (end != std::string_view::npos) {
        i = end + 1;
        continue;
      }
    }
    if (c == '[') {
      size_t close = text.find("](", i);
      size_t end = close == std::string_view::npos ? close : text.find(')', close);
      if (end != std::string_view::npos) {
        out.append(text.substr(i + 1, close - i - 1));
        i = end + 1;
        continue;
      }
    }
    out += c;
    ++i;
  }
  return out;
}

std::string_view Trim(std::string_view text) {
  while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.remove_prefix(1);
  while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r')) text.remove_suffix(1);
  return text;
}

// "- item", "* item", "1. item": the item's text, or npos.
size_t ListMarker(std::string_view line) {
  if (line.size() >= 2 && (line[0] == '-' || line[0] == '*' || line[0] == '+') && line[1] == ' ') return 2;
  size_t digits = 0;
  while (digits < line.size() && digits < 3 && line[digits] >= '0' && line[digits] <= '9') ++digits;
  if (digits && digits + 1 < line.size() && (line[digits] == '.' || line[digits] == ')') && line[digits + 1] == ' ') {
    return digits + 2;
  }
  return std::string_view::npos;
}

// "2026-10-25T18:03:00Z" as the language's date.
std::string PublishedDate(const std::string& iso, uint32_t language) {
  std::tm utc = {};
  if (sscanf_s(iso.c_str(), "%d-%d-%dT%d:%d:%d", &utc.tm_year, &utc.tm_mon, &utc.tm_mday, &utc.tm_hour,
                  &utc.tm_min, &utc.tm_sec) != 6) {
    return iso.substr(0, 10);
  }
  utc.tm_year -= 1900;
  utc.tm_mon -= 1;
  std::time_t seconds = _mkgmtime(&utc);
  constexpr uint64_t kUnixEpoch = 116'444'736'000'000'000ULL;
  return FormatDate(uint64_t(seconds) * 10'000'000ULL + kUnixEpoch, language);
}

std::string Megabytes(uint64_t bytes) {
  char text[32];
  std::snprintf(text, sizeof(text), "%.1f", double(bytes) / (1024.0 * 1024.0));
  return text;
}

}  // namespace

std::string App::LastCheckedText() const {
  int64_t when = settings_.o.last_update_check;
  if (when <= 0) return t_.never;
  constexpr uint64_t kUnixEpoch = 116'444'736'000'000'000ULL;
  std::string date = FormatDate(uint64_t(when) * 10'000'000ULL + kUnixEpoch, language_);
  std::time_t seconds = std::time_t(when);
  std::tm local = {};
  if (localtime_s(&local, &seconds) == 0) {
    char time_text[16];
    std::snprintf(time_text, sizeof(time_text), " %02d:%02d", local.tm_hour, local.tm_min);
    date += time_text;
  }
  return date;
}

float App::ReleaseNotes(const std::string& markdown, ImVec2 pos, float width, bool draw) {
  constexpr Type kText{Font::kRegular, 14};
  constexpr Type kHeading{Font::kSemiBold, 15};
  constexpr float kLine = 1.5f;
  float y = pos.y;
  std::string paragraph;
  int item_level = -1;  // the list item being gathered, or -1 for a paragraph
  bool quote = false;
  bool any = false;
  auto flush = [&] {
    if (paragraph.empty()) return;
    std::string text = PlainInline(paragraph);
    float indent = item_level >= 0 ? Px(18.0f) * float(item_level + 1) : quote ? Px(14.0f) : 0.0f;
    float height = DrawParagraph(kText, ImVec2(pos.x + indent, y), width - indent, kLine, col::kTextSoft, text, draw);
    if (draw && item_level >= 0) {
      float dot = Px(5.0f);
      ImVec2 center(pos.x + indent - Px(10.0f), y + Px(kText.size) * kLine * 0.5f);
      ImGui::GetWindowDrawList()->AddCircleFilled(center, dot * 0.5f, item_level ? col::kTextDim : col::kAccent);
    }
    if (draw && quote) {
      FillRect(ImRect(ImVec2(pos.x, y + Px(2.0f)), ImVec2(pos.x + Px(3.0f), y + height - Px(2.0f))), col::kOutline, 1.5f);
    }
    y += height + Px(item_level >= 0 ? 4.0f : 8.0f);
    paragraph.clear();
    any = true;
  };
  size_t start = 0;
  while (start <= markdown.size()) {
    size_t end = markdown.find('\n', start);
    std::string_view raw(markdown.data() + start, (end == std::string::npos ? markdown.size() : end) - start);
    start = end == std::string::npos ? markdown.size() + 1 : end + 1;
    size_t indent = 0;
    while (indent < raw.size() && (raw[indent] == ' ' || raw[indent] == '\t')) ++indent;
    std::string_view line = Trim(raw);
    if (line.empty()) {
      flush();
      item_level = -1;
      quote = false;
      continue;
    }
    if (line[0] == '#') {
      flush();
      item_level = -1;
      quote = false;
      while (!line.empty() && line[0] == '#') line.remove_prefix(1);
      std::string heading = PlainInline(Trim(line));
      if (any) y += Px(6.0f);
      if (draw) DrawLabel(kHeading, ImVec2(pos.x, y), col::kText, Ellipsize(kHeading, heading, width));
      y += LineHeight(kHeading) + Px(6.0f);
      any = true;
      continue;
    }
    if (line[0] == '>') {
      line.remove_prefix(1);
      line = Trim(line);
      if (line.rfind("[!", 0) == 0) continue;  // GitHub's "> [!IMPORTANT]" marker
      if (!quote) {
        flush();
        quote = true;
        item_level = -1;
      }
      if (!paragraph.empty()) paragraph += ' ';
      paragraph.append(line);
      continue;
    }
    if (size_t marker = ListMarker(line); marker != std::string_view::npos) {
      flush();
      quote = false;
      item_level = std::min<int>(int(indent / 2), 3);
      paragraph.append(line.substr(marker));
      continue;
    }
    if (quote) {
      flush();
      quote = false;
    }
    if (!paragraph.empty()) paragraph += ' ';
    paragraph.append(line);
  }
  flush();
  return y - pos.y;
}

void App::UpdatesPage(const ImRect& rect) {
  Options& o = settings_.o;
  ImRect content = PageContent(rect);
  float y = Header(content.Min, nullptr, t_.updates_title, t_.updates_subtitle) + Px(20.0f);
  constexpr Type kSection{Font::kBold, 12, 0.1f};
  constexpr Type kVersionType{Font::kCondensedBold, 34};
  constexpr Type kStatus{Font::kRegular, 14};
  bool changed = false;
  const Version running = Version::Parse(kVersion);
  const bool checking = update_check_.valid();
  const Update::Progress progress = update_.Get();
  const bool updating = progress.state == Update::State::kDownloading || progress.state == Update::State::kVerifying ||
                        progress.state == Update::State::kInstalling || progress.state == Update::State::kDone;
  const Release* newer = updates_ ? updates_->Newer(running) : nullptr;
  // A game without releases to look for (game.h): its version, and nothing to check.
  const bool off = update_url_.empty();

  // The installed version, the last check and its result.
  {
    float text_height = LineHeight(kSection) + Px(4.0f) + Px(34.0f) + Px(10.0f) + LineHeight(kStatus);
    float height = Px(28.0f) * 2.0f + std::max(Px(64.0f), text_height);
    ImRect card(ImVec2(content.Min.x, y), ImVec2(content.Max.x, y + height));
    FillRect(card, col::kCard, 12.0f);
    ImRect icon(ImVec2(card.Min.x + Px(28.0f), card.Min.y + (height - Px(64.0f)) * 0.5f),
                ImVec2(card.Min.x + Px(92.0f), card.Min.y + (height + Px(64.0f)) * 0.5f));
    FillRect(icon, col::kAccentTint, 12.0f);
    DrawIcon(Icon::kRefresh, ImVec2(icon.Min.x + Px(17.0f), icon.Min.y + Px(17.0f)), 30.0f, col::kAccent, 1.8f);
    float x = icon.Max.x + Px(24.0f);
    float top = card.Min.y + (height - text_height) * 0.5f;
    DrawLabel(kSection, ImVec2(x, top), col::kLabel, t_.installed_version);
    top += LineHeight(kSection) + Px(4.0f);
    DrawLabel(kVersionType, ImVec2(x, top), col::kText, kVersion, Px(34.0f));
    top += Px(34.0f) + Px(10.0f);

    ButtonSpec check{ButtonKind::kPrimary, Type{Font::kBold, 16}, 52, 28, 10, Icon::kRefresh, 18, 2};
    check.gap = 10;
    float button_width = off ? 0.0f : ButtonWidth(check, t_.check_updates);
    float status_width = card.Max.x - Px(56.0f) - button_width - x;
    if (off) {
      DrawLabel(kStatus, ImVec2(x, top), col::kTextMuted, Ellipsize(kStatus, t_.updates_off, status_width));
    } else if (checking) {
      DrawLabel(kStatus, ImVec2(x, top), col::kTextMuted, t_.checking_updates);
    } else if (updates_ && !updates_->ok) {
      char text[512];
      if (updates_->rate_limited) {
        std::snprintf(text, sizeof(text), "%s", t_.check_rate_limited);
      } else {
        std::snprintf(text, sizeof(text), t_.check_failed, updates_->error.c_str());
      }
      DrawLabel(kStatus, ImVec2(x, top), col::kError, Ellipsize(kStatus, text, status_width));
    } else {
      char checked[128];
      std::snprintf(checked, sizeof(checked), t_.last_check, LastCheckedText().c_str());
      DrawLabel(kStatus, ImVec2(x, top), col::kTextMuted, checked);
      if (updates_ && !newer) {
        float after = x + MeasureText(kStatus, checked).x;
        DrawLabel(kStatus, ImVec2(after, top), col::kTextDim, "  ·  ");
        after += MeasureText(kStatus, "  ·  ").x;
        DrawLabel(kStatus, ImVec2(after, top), col::kSuccess,
                  Ellipsize(kStatus, t_.up_to_date, std::max(0.0f, x + status_width - after)));
      }
    }
    if (!off && Button("check", ImVec2(card.Max.x - Px(28.0f) - button_width, card.Min.y + (height - Px(52.0f)) * 0.5f),
                       check, t_.check_updates, !checking && !updating)) {
      CheckForUpdatesNow();
    }
    y += height + Px(20.0f);
  }

  // A new version: its tag, date and size, and its installation.
  if (newer) {
    float text_height = LineHeight(kSection) + Px(4.0f) + Px(34.0f) + Px(10.0f) + LineHeight(kStatus);
    float height = Px(28.0f) * 2.0f + text_height;
    bool failed = progress.state == Update::State::kFailed;
    if (updating || failed) {
      height += Px(18.0f) + LineHeight(kStatus);  // the progress or the error
    }
    ImRect card(ImVec2(content.Min.x, y), ImVec2(content.Max.x, y + height));
    FillRect(card, col::kCard, 12.0f);
    StrokeRect(card, Fade(col::kAccent, 0.6f), 12.0f, 1.5f);
    float x = card.Min.x + Px(28.0f);
    float top = card.Min.y + Px(28.0f);
    DrawLabel(kSection, ImVec2(x, top), col::kAccent, t_.update_available);
    top += LineHeight(kSection) + Px(4.0f);
    DrawLabel(kVersionType, ImVec2(x, top), col::kText, newer->tag, Px(34.0f));
    top += Px(34.0f) + Px(10.0f);
    char published[160];
    std::snprintf(published, sizeof(published), t_.update_published, PublishedDate(newer->published, language_).c_str(),
                  Megabytes(newer->asset_size).c_str());
    DrawLabel(kStatus, ImVec2(x, top), col::kTextMuted, published);
    top += LineHeight(kStatus) + Px(18.0f);

    ButtonSpec install{ButtonKind::kPrimary, Type{Font::kBold, 16}, 52, 28, 10, Icon::kArrowDown, 18, 2};
    install.gap = 10;
    float button_y = card.Min.y + Px(28.0f) + (text_height - Px(52.0f)) * 0.5f;
    if (updating) {
      // The progress: the bar and what it is doing; the download can be cancelled.
      char status[160];
      if (progress.state == Update::State::kDownloading) {
        std::snprintf(status, sizeof(status), t_.update_downloading, Megabytes(progress.done).c_str(),
                      Megabytes(progress.total).c_str());
      } else {
        std::snprintf(status, sizeof(status), "%s",
                      progress.state == Update::State::kVerifying ? t_.update_verifying : t_.update_installing);
      }
      DrawLabel(kStatus, ImVec2(x, top), col::kText, status);
      float hint_width = MeasureText(kStatus, t_.update_restart_hint).x;
      DrawLabel(kStatus, ImVec2(card.Max.x - Px(28.0f) - hint_width, top), col::kTextDim, t_.update_restart_hint);
      float fraction = progress.state == Update::State::kDownloading && progress.total
                           ? float(double(progress.done) / double(progress.total))
                           : 1.0f;
      float bar_y = top - Px(10.0f);
      Progress(ImRect(ImVec2(x, bar_y), ImVec2(card.Max.x - Px(28.0f), bar_y + Px(6.0f))), fraction);
      if (progress.state == Update::State::kDownloading) {
        ButtonSpec cancel{ButtonKind::kGhost, Type{Font::kSemiBold, 15}, 52, 24, 10};
        float cancel_width = ButtonWidth(cancel, t_.cancel);
        if (Button("cancel_update", ImVec2(card.Max.x - Px(28.0f) - cancel_width, button_y), cancel, t_.cancel)) {
          update_.Cancel();
        }
      }
    } else {
      if (failed) {
        // Why, in the launcher's language when it is one of the expected failures.
        char reason[384];
        switch (progress.problem) {
          case Update::Problem::kNoChecksum:
            std::snprintf(reason, sizeof(reason), "%s", t_.update_no_checksum);
            break;
          case Update::Problem::kChecksumMismatch:
            std::snprintf(reason, sizeof(reason), "%s", t_.update_checksum_mismatch);
            break;
          case Update::Problem::kInUse:
            std::snprintf(reason, sizeof(reason), t_.update_in_use, progress.detail.c_str());
            break;
          default:
            std::snprintf(reason, sizeof(reason), "%s", progress.error.c_str());
            break;
        }
        char text[512];
        std::snprintf(text, sizeof(text), t_.update_failed, reason);
        DrawLabel(kStatus, ImVec2(x, top), col::kError, Ellipsize(kStatus, text, card.GetWidth() - Px(56.0f)));
      }
      float width = ButtonWidth(install, t_.update_now);
      if (Button("install", ImVec2(card.Max.x - Px(28.0f) - width, button_y), install, t_.update_now, !checking)) {
        update_.Start(*newer, exe_dir_);
      }
    }
    y += height + Px(20.0f);
  }

  // Checking when the launcher opens.
  if (!off) {
    constexpr Type kTitle{Font::kSemiBold, 15};
    constexpr Type kHint{Font::kRegular, 13};
    float height = Px(20.0f) * 2.0f + LineHeight(kTitle) + Px(4.0f) + LineHeight(kHint);
    ImRect card(ImVec2(content.Min.x, y), ImVec2(content.Max.x, y + height));
    FillRect(card, col::kCard, 12.0f);
    DrawLabel(kTitle, ImVec2(card.Min.x + Px(28.0f), card.Min.y + Px(20.0f)), col::kText, t_.check_on_start);
    DrawLabel(kHint, ImVec2(card.Min.x + Px(28.0f), card.Min.y + Px(20.0f) + LineHeight(kTitle) + Px(4.0f)),
              col::kTextMuted, t_.check_on_start_hint);
    if (Toggle("auto", ImVec2(card.Max.x - Px(28.0f) - Px(52.0f), card.Min.y + (height - Px(30.0f)) * 0.5f),
               &o.check_updates)) {
      changed = true;
    }
    y += height + Px(20.0f);
  }

  // The release notes: what is new since the installed version, or the installed version's.
  if (!off) {
    ImRect card(ImVec2(content.Min.x, y), content.Max);
    FillRect(card, col::kCard, 12.0f);
    constexpr Type kLinkType{Font::kSemiBold, 14};
    float x = card.Min.x + Px(28.0f);
    float link_y = card.Max.y - Px(24.0f) - LineHeight(kLinkType);
    if (Link("history", ImVec2(x, link_y), t_.full_history)) {
      OpenUrl(game::kReleasesPage);
    }
    ImRect notes(ImVec2(x, card.Min.y + Px(24.0f)), ImVec2(card.Max.x - Px(16.0f), link_y - Px(16.0f)));
    std::vector<const Release*> shown;
    if (updates_ && updates_->ok) {
      for (const Release& release : updates_->releases) {
        if (running < release.version) shown.push_back(&release);
      }
      if (shown.empty()) {
        if (const Release* installed = updates_->Of(running)) shown.push_back(installed);
      }
    }
    if (shown.empty()) {
      char title[96];
      std::snprintf(title, sizeof(title), t_.release_notes, kVersion);
      DrawLabel(kSection, notes.Min, col::kLabel, title);
      DrawParagraph(Type{Font::kRegular, 14}, ImVec2(notes.Min.x, notes.Min.y + LineHeight(kSection) + Px(14.0f)),
                    notes.GetWidth() - Px(12.0f), 1.5f, col::kTextSoft,
                    checking ? t_.checking_updates : t_.release_notes_none);
    } else if (notes.GetHeight() > Px(40.0f)) {
      BeginScroll("notes", notes);
      ImVec2 origin = ImGui::GetCursorScreenPos();
      float width = ImGui::GetContentRegionAvail().x - Px(20.0f);
      float top = origin.y;
      for (const Release* release : shown) {
        char title[96];
        std::snprintf(title, sizeof(title), running < release->version ? t_.whats_new : t_.release_notes,
                      release->tag.c_str());
        if (top > origin.y) top += Px(14.0f);
        DrawLabel(kSection, ImVec2(origin.x, top), running < release->version ? col::kAccent : col::kLabel, title);
        top += LineHeight(kSection) + Px(14.0f);
        top += ReleaseNotes(release->notes, ImVec2(origin.x, top), width, true);
      }
      ImGui::Dummy(ImVec2(width, top - origin.y));
      EndScroll();
    }
  }
  if (changed) {
    Changed();
  }
}

}  // namespace launcher
