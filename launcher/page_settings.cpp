// The Settings pages: Video (monitor, display mode, resolution, image format, internal resolution) and
// Graphics (the renderer, frame rate, antialiasing, anisotropic filtering, shadow quality and, with Xenos, the
// graphics API). Every change is saved at once.

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "app.h"
#include "pages.h"

namespace launcher {

using namespace ui;

namespace {

std::string SizeText(Size size) { return std::to_string(size.first) + " × " + std::to_string(size.second); }

const SegmentedSpec kVideoSegments{40, 18, 0};
const SegmentedSpec kGraphicsSegments{40, 16, 64};

}  // namespace

void App::VideoPage(const ImRect& rect) {
  Options& o = settings_.o;
  ImRect content = PageContent(rect);
  float y = Header(content.Min, t_.settings, t_.nav_video, t_.video_subtitle) + Px(20.0f);
  float width = content.GetWidth();
  float combo_width = std::min(Px(420.0f), width - Px(28.0f + 300.0f + 24.0f + 28.0f));
  ImVec2 at;
  bool changed = false;
  Card card;
  float top = y;

  // Monitor.
  {
    std::vector<std::string> items;
    for (size_t i = 0; i < settings_.monitors.size(); ++i) {
      const Monitor& m = settings_.monitors[i];
      std::string text = std::to_string(i + 1) + ": " + (m.name.empty() ? std::string(t_.monitor) : m.name) +
                         " — " + SizeText(m.size);
      if (m.primary) {
        text += std::string(" (") + t_.primary + ")";
      }
      items.push_back(std::move(text));
    }
    y += Row(ImVec2(content.Min.x, y), width, 20, t_.monitor, t_.monitor_hint, 44, false, &at);
    int selected = std::clamp(o.monitor, 1, int(settings_.monitors.size())) - 1;
    if (Combo("monitor", ImRect(at, ImVec2(at.x + combo_width, at.y + Px(44.0f))), items, &selected)) {
      o.monitor = selected + 1;
      settings_.MonitorChanged();
      changed = true;
    }
  }

  // Display mode.
  {
    std::vector<std::string> items = {t_.fullscreen, t_.borderless, t_.window};
    y += Row(ImVec2(content.Min.x, y), width, 20, t_.display, t_.display_hint, 48, false, &at);
    if (Segmented("display", at, kVideoSegments, items, &o.display)) {
      changed = true;
    }
  }

  // Resolution: the window's size, or the monitor's in fullscreen and borderless.
  {
    bool window = o.display == kWindow;
    std::vector<std::string> items;
    int selected = 0;
    if (window) {
      for (size_t i = 0; i < settings_.window_sizes.size(); ++i) {
        items.push_back(SizeText(settings_.window_sizes[i]));
        if (settings_.window_sizes[i] == o.window) selected = int(i);
      }
    } else {
      items.push_back(SizeText(settings_.CurrentMonitor().size) + " " + t_.monitor_resolution);
    }
    y += Row(ImVec2(content.Min.x, y), width, 20, t_.resolution,
             window ? t_.resolution_hint_window : t_.resolution_hint_screen, 44, false, &at);
    if (Combo("resolution", ImRect(at, ImVec2(at.x + combo_width, at.y + Px(44.0f))), items, &selected, window) &&
        window && selected < int(settings_.window_sizes.size())) {
      o.window = settings_.window_sizes[selected];
      changed = true;
    }
  }

  // Image format: NativeXRenderer fills an output that is not 16:9, or keeps 16:9 with bars (D43, D54); Xenos
  // keeps 16:9 with bars, unless the game widens its cameras (game::kUltrawideCvar: Xenos stretches then).
  Size output = settings_.OutputSize();
  {
    bool wide = !Is16x9(output);
    bool applies = o.native || *game::kUltrawideCvar;
    bool enabled = applies && wide;
    std::vector<std::string> items = {t_.format_fill, t_.format_bars};
    int selected = !applies ? 1 : o.native_fill ? 0 : 1;
    const char* hint = !applies ? t_.format_hint_xenos : wide ? t_.format_hint : t_.format_hint_16x9;
    y += Row(ImVec2(content.Min.x, y), width, 20, t_.format, hint, 48, false, &at);
    if (Segmented("format", at, kVideoSegments, items, &selected, enabled) && enabled) {
      o.native_fill = selected == 0;
      changed = true;
    }
  }

  // Internal resolution: NativeXRenderer's (automatic, or a lower height in the chosen format), or Xenos's scale.
  {
    std::vector<std::string> items;
    int selected = 0;
    std::vector<int> heights;
    if (o.native) {
      heights = NativeHeights(output, o.native_fill, o.native_height);
      if (std::find(heights.begin(), heights.end(), o.native_height) == heights.end()) {
        o.native_height = 0;  // another monitor or window no longer offers it
      }
      for (size_t i = 0; i < heights.size(); ++i) {
        Size size = NativeRenderSize(output, o.native_fill, heights[i]);
        std::string text = SizeText(size);
        if (!heights[i]) {
          text = std::string(t_.automatic) + " — " + text;
        } else if (size == Size{kGuestWidth, kGuestHeight}) {
          text += std::string(" ") + t_.original;
        }
        items.push_back(std::move(text));
        if (heights[i] == o.native_height) selected = int(i);
      }
    } else {
      auto render = [&](Size scale) {
        std::string text = SizeText({kGuestWidth * scale.first, kGuestHeight * scale.second});
        if (scale.first == 1 && scale.second == 1) return text + " " + t_.original;
        if (scale.first == scale.second) return text + " (" + std::to_string(scale.first) + "×)";
        return text + " (" + std::to_string(scale.first) + "×" + std::to_string(scale.second) + ")";
      };
      items.push_back(std::string(t_.automatic) + " — " + render(AutoScale(output)));
      for (int scale = 1; scale <= kMaxScale; ++scale) {
        items.push_back(render({scale, scale}));
      }
      selected = o.scale;
    }
    char xenos_hint[128];
    std::snprintf(xenos_hint, sizeof(xenos_hint), t_.internal_hint_xenos, game::kFrameWidth, game::kFrameHeight);
    y += Row(ImVec2(content.Min.x, y), width, 20, t_.internal, o.native ? t_.internal_hint : xenos_hint, 44, true, &at);
    if (Combo("internal", ImRect(at, ImVec2(at.x + combo_width, at.y + Px(44.0f))), items, &selected)) {
      if (o.native) {
        o.native_height = heights[size_t(selected)];
      } else {
        o.scale = std::clamp(selected, 0, kMaxScale);
      }
      changed = true;
    }
  }
  card.End(ImRect(ImVec2(content.Min.x, top), ImVec2(content.Max.x, y)));

  if (Footer(content, t_.reset_values)) {
    ResetVideo(o);
    settings_.MonitorChanged();
    changed = true;
  }
  if (changed) {
    Changed();
  }
}

void App::GraphicsPage(const ImRect& rect) {
  Options& o = settings_.o;
  ImRect content = PageContent(rect);
  float y = Header(content.Min, t_.settings, t_.nav_graphics, nullptr);
  bool changed = false;

  // The API in use, right of the title.
  {
    constexpr Type kLabel{Font::kRegular, 13};
    constexpr Type kValue{Font::kSemiBold, 13};
    std::string label = std::string(t_.graphics_api) + ": ";
    const char* api = o.native || o.vulkan ? "Vulkan" : "DirectX 12";
    float badge_width = MeasureText(kLabel, label).x + MeasureText(kValue, api).x + Px(24.0f);
    float badge_height = LineHeight(kLabel) + Px(12.0f);
    ImRect badge(ImVec2(content.Max.x - badge_width, y - badge_height), ImVec2(content.Max.x, y));
    FillRect(badge, col::kCardRaised, 6.0f);
    DrawLabel(kLabel, ImVec2(badge.Min.x + Px(12.0f), badge.Min.y + Px(6.0f)), col::kTextMuted, label);
    DrawLabel(kValue, ImVec2(badge.Min.x + Px(12.0f) + MeasureText(kLabel, label).x, badge.Min.y + Px(6.0f)), col::kText,
             api);
  }
  y += Px(18.0f);

  // The renderer: two cards, when NativeXRenderer draws this game.
  if (game::kHasNativeXRenderer) {
    float gap = Px(14.0f);
    float card_width = (content.GetWidth() - gap) * 0.5f;
    constexpr Type kTitle{Font::kBold, 17};
    constexpr Type kText{Font::kRegular, 13};
    const char* native_text = settings_.has_vulkan ? t_.native_text : t_.native_missing;
    float text_width = card_width - Px(40.0f);
    float text_height = std::max(DrawParagraph(kText, ImVec2(0, 0), text_width, 1.45f, 0, native_text, false),
                                 DrawParagraph(kText, ImVec2(0, 0), text_width, 1.45f, 0, t_.xenos_text, false));
    float title_height = std::max(LineHeight(kTitle), Px(18.0f));
    float card_height = Px(18.0f) * 2.0f + title_height + Px(8.0f) + text_height;
    for (int i = 0; i < 2; ++i) {
      bool native = i == 0;
      bool enabled = !native || settings_.has_vulkan;
      ImRect r(ImVec2(content.Min.x + float(i) * (card_width + gap), y),
               ImVec2(content.Min.x + float(i) * (card_width + gap) + card_width, y + card_height));
      ItemState state = Item(native ? "native" : "xenos", r, enabled);
      bool on = o.native == native;
      ImU32 background = on ? col::kCardSelected : col::kCard;
      ImU32 border = on ? col::kAccent : col::kSidebarBorder;
      if (state.hovered) {
        background = Brighten(background);
        border = Brighten(border);
      }
      float alpha = enabled ? 1.0f : 0.5f;
      FillRect(r, Fade(background, alpha), 12.0f);
      StrokeRect(r, Fade(border, alpha), 12.0f, 2.0f);
      float x = r.Min.x + Px(20.0f);
      float top = r.Min.y + Px(18.0f);
      ImVec2 center(x + Px(9.0f), top + title_height * 0.5f);
      if (on) {
        ImGui::GetWindowDrawList()->AddCircle(center, Px(9.0f) - Px(2.5f), col::kAccent, 24, Px(5.0f));
      } else {
        ImGui::GetWindowDrawList()->AddCircle(center, Px(9.0f) - Px(1.0f), Fade(col::kRadioOff, alpha), 24, Px(2.0f));
      }
      const char* title = native ? "NativeXRenderer" : "Xenos";
      DrawLabel(kTitle, ImVec2(x + Px(28.0f), top), Fade(col::kText, alpha), title, title_height);
      if (native) {
        ImVec2 badge = BadgeSize(t_.recommended);
        Badge(ImVec2(x + Px(28.0f) + MeasureText(kTitle, title).x + Px(10.0f), top + (title_height - badge.y) * 0.5f),
              t_.recommended, Fade(col::kAccent, alpha), col::kOnAccent);
      }
      DrawParagraph(kText, ImVec2(x, top + title_height + Px(8.0f)), text_width, 1.45f, Fade(col::kTextMuted, alpha),
                      native ? native_text : t_.xenos_text);
      if (state.focused) {
        FocusRing(r, 12.0f);
      }
      if (state.pressed && !on) {
        o.native = native;
        changed = true;
      }
    }
    y += card_height + Px(18.0f);
  }

  // The options of the renderer chosen.
  float width = content.GetWidth();
  ImVec2 at;
  Card card;
  float top = y;
  if (*game::kFrameRateCvar) {
    // The rates the monitor reaches; those above 60 are experimental.
    std::vector<game::FrameRate> rates = FrameRates(settings_.CurrentMonitor());
    std::vector<std::string> items;
    int selected = 0;
    for (size_t i = 0; i < rates.size(); ++i) {
      items.push_back(FrameRateText(rates[i].fps, t_.experimental));
      if (rates[i].value == o.frame_rate) selected = int(i);
    }
    y += Row(ImVec2(content.Min.x, y), width, 16, t_.framerate, t_.framerate_hint, 48, false, &at);
    if (Segmented("fps", at, kGraphicsSegments, items, &selected)) {
      o.frame_rate = rates[size_t(selected)].value;
      changed = true;
    }
  }
  if (o.native) {
    // The mockup's order: SMAA, FXAA, DLAA, none; an experimental DLSS mode the options file named, last.
    constexpr int kOrder[] = {kSmaa, kFxaa, kDlaa, kAaOff};
    std::vector<std::string> items = {"SMAA", "FXAA", "DLAA", t_.none};
    if (o.native_aa_experimental >= 0) {
      items.push_back(std::string(kExperimentalAntialiasing[o.native_aa_experimental].label) + " (" + t_.experimental +
                      ")");
    }
    int selected = o.native_aa >= kNativeAntialiasingCount
                       ? 4
                       : int(std::find(std::begin(kOrder), std::end(kOrder), o.native_aa) - std::begin(kOrder));
    y += Row(ImVec2(content.Min.x, y), width, 16, t_.antialiasing, t_.antialiasing_hint, 48, false, &at);
    if (Segmented("aa", at, kGraphicsSegments, items, &selected)) {
      o.native_aa = selected < 4 ? kOrder[selected] : kNativeAntialiasingCount;
      changed = true;
    }
  } else if (*game::kNoMsaaCvar) {
    std::vector<std::string> items = {game::kMsaaLabel, t_.none};
    int selected = o.no_msaa ? 1 : 0;
    y += Row(ImVec2(content.Min.x, y), width, 16, t_.antialiasing, t_.antialiasing_hint_xenos, 48, false, &at);
    if (Segmented("msaa", at, kGraphicsSegments, items, &selected)) {
      o.no_msaa = selected == 1;
      changed = true;
    }
  }
  {
    constexpr int kValues[] = {0, 2, 3, 4, 5};
    std::vector<std::string> items = {t_.off, "2×", "4×", "8×", "16×"};
    int selected = int(std::find(std::begin(kValues), std::end(kValues), o.anisotropic) - std::begin(kValues));
    y += Row(ImVec2(content.Min.x, y), width, 16, t_.anisotropic, t_.anisotropic_hint, 48,
             o.native && !game::kShadowQuality, &at);
    if (Segmented("anisotropic", at, kGraphicsSegments, items, &selected)) {
      o.anisotropic = kValues[selected];
      changed = true;
    }
  }
  if (game::kShadowQuality) {
    // The shadow maps' resolution, NativeXRenderer's (its D92): the game's scaled like everything else, or at least
    // twice or four times the game's. Shown with Xenos, disabled.
    std::vector<std::string> items = {"Original", t_.shadows_high, t_.shadows_ultra};
    int selected = o.native ? o.shadow_quality : 0;
    y += Row(ImVec2(content.Min.x, y), width, 16, t_.shadows, o.native ? t_.shadows_hint : t_.shadows_hint_xenos, 48,
             o.native, &at);
    if (Segmented("shadows", at, kGraphicsSegments, items, &selected, o.native)) {
      o.shadow_quality = selected;
      changed = true;
    }
  }
  if (!o.native) {
    // Xenos draws through the SDK's Direct3D 12 or Vulkan backend.
    std::vector<std::string> items = {"DirectX 12"};
    if (settings_.has_vulkan) {
      items.push_back("Vulkan");
    }
    int selected = o.vulkan ? 1 : 0;
    y += Row(ImVec2(content.Min.x, y), width, 16, t_.graphics_api, t_.api_hint, 48, true, &at);
    if (Segmented("api", at, kGraphicsSegments, items, &selected)) {
      o.vulkan = settings_.has_vulkan && selected == 1;
      changed = true;
    }
  }
  card.End(ImRect(ImVec2(content.Min.x, top), ImVec2(content.Max.x, y)));

  if (Footer(content, t_.reset_values)) {
    ResetGraphics(o);
    o.native = o.native && settings_.has_vulkan && game::kHasNativeXRenderer;
    changed = true;
  }
  if (changed) {
    Changed();
  }
}

}  // namespace launcher
