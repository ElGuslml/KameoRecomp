// The launcher's look: the type, the icons and the widgets of Gustavo's mockup, drawn with Dear ImGui, in the
// game's palette (palette.h, each game's own).
//
// Everything is laid out in the mockup's units ("dp": its CSS pixels, for a 1280x760 window) and drawn at
// Scale() pixels per dp, which follows the window (app.cpp). Widgets take absolute rectangles in pixels,
// like the mockup's flex boxes computed by hand, and register an ImGui item each, so the mouse, the
// keyboard and any SDL controller drive them through ImGui's navigation. Nothing here is specific to a game.

#pragma once

#include <imgui.h>
#include <imgui_internal.h>

#include <cmath>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "palette.h"
#include "ui/color.h"

namespace ui {

// The color with its alpha multiplied by `alpha`.
ImU32 Fade(ImU32 color, float alpha);
// CSS filter: brightness(1.15), the mockup's hover.
ImU32 Brighten(ImU32 color);

// --- Units ---

void SetScale(float pixels_per_dp);
float Scale();
inline float Px(float dp) { return dp * Scale(); }
inline float Snap(float px) { return std::floor(px + 0.5f); }
inline ImVec2 Snap(ImVec2 p) { return {Snap(p.x), Snap(p.y)}; }

// --- Type ---

enum class Font : int { kRegular, kMedium, kSemiBold, kBold, kCondensedSemiBold, kCondensedBold, kCount };

// Barlow 400/500/600/700 and Barlow Condensed 600/700 (SIL OFL 1.1, launcher/fonts), embedded in the
// executable. Sizes are em sizes, as in CSS. False if a font is missing.
bool LoadFonts();

struct Type {
  Font font = Font::kRegular;
  float size = 14;      // dp
  float tracking = 0;   // letter spacing, in em
};

// The height of one line at "line-height: normal" (Barlow: 1.2 em), in pixels.
float LineHeight(const Type& type);
// The size of one line of text, in pixels.
ImVec2 MeasureText(const Type& type, std::string_view text);
// One line. `pos` is the top of its line box; `line_height` (pixels, 0 = normal) centers the glyphs in it,
// as CSS does.
void DrawLabel(const Type& type, ImVec2 pos, ImU32 color, std::string_view text, float line_height = 0);
// Text wrapped at `width` pixels, `line_height` em per line, each line aligned by `align` (0 left, 0.5
// centered). Returns its height in pixels; with `draw` false it only measures.
float DrawParagraph(const Type& type, ImVec2 pos, float width, float line_height, ImU32 color,
                      std::string_view text, bool draw = true, float align = 0.0f);
// The text cut with "…" to fit `width` pixels.
std::string Ellipsize(const Type& type, std::string_view text, float width);

// --- Pictures ---

// The mockup's stroke icons (24 units, round caps and joins).
enum class Icon : int {
  kNone,
  kPlay,
  kTrophy,
  kGamepad,
  kSliders,
  kRefresh,
  kReset,
  kHeart,
  kBrand,  // the game's mark (game.h, kBrandIcon)
  kCheck,
  kArrowRight,
  kArrowLeft,
  kArrowUp,
  kArrowDown,
  kChevronDown,
  kLock,
  kLogOut,
  kDisc,
  kFolder,
  kDrive,
  kUpload,
  kAlert,
  kMouse,
  kKeyboard,
  kPsCross,
  kPsCircle,
  kPsSquare,
  kPsTriangle,
  kCount
};

// An icon in a size x size dp box whose top left is `pos` (pixels): stroked `stroke` units wide, or filled.
void DrawIcon(Icon icon, ImVec2 pos, float size, ImU32 color, float stroke = 1.8f, bool filled = false);

// The mockup's art (launcher/art), in the executable's resources.
enum class Art : int { kHero, kInstall, kCount };
// The art covering `rect` (CSS "slice"), cropped around `anchor` (0..1 per axis), with rounded corners.
void DrawArt(Art art, const ImRect& rect, ImVec2 anchor, float rounding = 0, ImDrawFlags corners = 0);

// Pictures and icons are drawn at the current scale: drop them when it changes.
void ReleasePictures();

// A resource of the executable (RCDATA), or empty.
std::string_view Resource(int id);

// --- Items ---

struct ItemState {
  bool hovered = false;
  bool held = false;
  bool pressed = false;
  bool focused = false;  // with the keyboard or a controller (the focus ring shows)
};

// Registers an interactive rectangle (pixels) with ImGui: mouse, keyboard and controller. A disabled
// item is drawn but cannot be focused or pressed.
ItemState Item(const char* id, const ImRect& rect, bool enabled = true);
ItemState Item(ImGuiID id, const ImRect& rect, bool enabled = true);
// The mockup's :focus-visible outline: 2 dp of accent, 2 dp outside the item.
void FocusRing(const ImRect& rect, float rounding);
// A controller button is held (ImGui's gamepad keys).
bool AnyGamepadButtonDown();
// The first interactive item registered between the two calls (a page's first control).
void CaptureFirstItem();
ImGuiID CapturedFirstItem();

// --- Widgets ---

enum class ButtonKind {
  kPrimary,    // accent
  kHero,       // the Play page's dark secondary button
  kOutline,    // card background, outlined
  kGhost,      // transparent, outlined
  kLight,      // light background
  kDonate,     // coral
  kDisabled,   // the mockup's disabled primary (Continue, Extract before a valid ISO)
};

struct ButtonSpec {
  ButtonKind kind = ButtonKind::kOutline;
  Type type{Font::kSemiBold, 14};
  float height = 44;
  float padding = 18;
  float radius = 8;
  Icon icon = Icon::kNone;
  float icon_size = 18;
  float icon_stroke = 2;
  bool icon_filled = false;
  bool icon_after = false;  // the icon right of the label (arrows)
  float gap = 8;
};

float ButtonWidth(const ButtonSpec& spec, std::string_view label);
// A button at `pos` (pixels, top left); `width` 0 = its natural width. True when pressed.
bool Button(const char* id, ImVec2 pos, const ButtonSpec& spec, std::string_view label, bool enabled = true,
            float width = 0);

struct SegmentedSpec {
  float height = 40;
  float padding = 18;    // per option, each side
  float min_width = 0;   // per option
  ImU32 background = col::kSegment;
};

// The mockup's segmented buttons: one option highlighted in accent. Returns true when `selected` changed.
ImVec2 SegmentedSize(const SegmentedSpec& spec, std::span<const std::string> labels);
bool Segmented(const char* id, ImVec2 pos, const SegmentedSpec& spec, std::span<const std::string> labels,
               int* selected, bool enabled = true);

// The mockup's switch, 52 x 30 dp. True when toggled.
bool Toggle(const char* id, ImVec2 pos, bool* value, bool enabled = true);

// A drop-down list in the mockup's select style. True when `selected` changed.
bool Combo(const char* id, const ImRect& rect, std::span<const std::string> items, int* selected,
           bool enabled = true);

// A slider (accent track and knob); left/right step it with the keyboard or a controller. True on change.
bool Slider(const char* id, const ImRect& rect, float* value, float min, float max, float step,
            bool enabled = true);

// An accent link with an arrow ("Ver logros →"). True when pressed.
bool Link(const char* id, ImVec2 pos, std::string_view label, float size = 14);

// A rounded label ("RECOMENDADO", "PRÓXIMAMENTE").
ImVec2 BadgeSize(std::string_view label, float size = 11);
void Badge(ImVec2 pos, std::string_view label, ImU32 background, ImU32 color, float size = 11);

// A progress bar.
void Progress(const ImRect& rect, float fraction, ImU32 track = col::kTrack, ImU32 fill = col::kAccent);

// A scrolling region (an ImGui child window that controllers can navigate into). Items inside it lay out
// from ImGui::GetCursorScreenPos() and end with ImGui::Dummy() for the content's height.
void BeginScroll(const char* id, const ImRect& rect);
void EndScroll();

// Rounded rectangles on the current window.
void FillRect(const ImRect& rect, ImU32 color, float rounding_dp = 0, ImDrawFlags corners = 0);
void StrokeRect(const ImRect& rect, ImU32 color, float rounding_dp = 0, float thickness_dp = 1);
void DashedRect(const ImRect& rect, ImU32 color, float rounding_dp, float dash_dp = 4, float gap_dp = 3);

}  // namespace ui
