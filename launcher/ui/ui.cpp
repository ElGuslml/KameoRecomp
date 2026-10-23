// The launcher's look (ui.h).

#include "ui/ui.h"

#include "game.h"

#include "resources.h"
#include "ui/gfx.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <map>
#include <tuple>
#include <vector>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

namespace ui {
namespace {

float g_scale = 1.0f;
bool g_capture_first = false;
ImGuiID g_first_item = 0;
ImFont* g_fonts[int(Font::kCount)] = {};
NSVGrasterizer* g_rasterizer = nullptr;

ImFont* FontOf(const Type& type) { return g_fonts[int(type.font)]; }

// Barlow's ascent minus its descent (hhea and typo metrics: 1000 and -200 of 1000 units), in em: CSS's
// "line-height: normal", and the size ImGui gives the font for a CSS font size.
constexpr float kBarlowHeight = 1.2f;
float FontPixels(const Type& type) { return Px(type.size) * kBarlowHeight; }

// The paths of each icon, in a 24-unit box (the mockup's, which follow Lucide's style; drawn here
// from scratch where the mockup had none: the mouse and the keyboard).
constexpr const char* kIconBodies[int(Icon::kCount)] = {
    /* kNone */ "",
    /* kPlay */ "<path d='M7 4l13 8-13 8z'/>",
    /* kTrophy */
    "<path d='M8 21h8'/><path d='M12 17v4'/><path d='M7 4h10v5a5 5 0 0 1-10 0z'/>"
    "<path d='M17 6h3v1a4 4 0 0 1-3.5 4'/><path d='M7 6H4v1a4 4 0 0 0 3.5 4'/>",
    /* kGamepad */
    "<rect x='2' y='6' width='20' height='12' rx='6'/><path d='M6 12h4'/><path d='M8 10v4'/>"
    "<path d='M15 11h.01'/><path d='M18 13h.01'/>",
    /* kSliders */
    "<path d='M4 6h9'/><path d='M19 6h1'/><path d='M4 12h3'/><path d='M13 12h7'/><path d='M4 18h11'/>"
    "<circle cx='16' cy='6' r='2.5'/><circle cx='10' cy='12' r='2.5'/><circle cx='18' cy='18' r='2.5'/>",
    /* kRefresh */ "<path d='M20 12a8 8 0 1 1-2.4-5.7'/><path d='M20 4v5h-5'/>",
    /* kReset */ "<path d='M4 12a8 8 0 1 0 2.4-5.7'/><path d='M4 4v5h5'/>",
    /* kHeart */ "<path d='M12 20.5s-8-4.6-8-10.6A4.4 4.4 0 0 1 12 7.3a4.4 4.4 0 0 1 8 2.6c0 6-8 10.6-8 10.6z'/>",
    /* kBrand */ launcher::game::kBrandIcon,
    /* kCheck */ "<path d='M5 12l5 5 9-10'/>",
    /* kArrowRight */ "<path d='M5 12h14'/><path d='M13 6l6 6-6 6'/>",
    /* kArrowLeft */ "<path d='M19 12H5'/><path d='M11 6l-6 6 6 6'/>",
    /* kArrowUp */ "<path d='M12 19V5'/><path d='M6 11l6-6 6 6'/>",
    /* kArrowDown */ "<path d='M12 5v14'/><path d='M6 13l6 6 6-6'/>",
    /* kChevronDown */ "<path d='M6 9l6 6 6-6'/>",
    /* kLock */ "<rect x='5' y='11' width='14' height='10' rx='2'/><path d='M8 11V8a4 4 0 0 1 8 0v3'/>",
    /* kLogOut */ "<path d='M9 20H6a2 2 0 0 1-2-2V6a2 2 0 0 1 2-2h3'/><path d='M16 16l4-4-4-4'/><path d='M20 12H9'/>",
    /* kDisc */ "<circle cx='12' cy='12' r='9'/><circle cx='12' cy='12' r='2.5'/>",
    /* kFolder */ "<path d='M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z'/>",
    /* kDrive */ "<rect x='3' y='13' width='18' height='7' rx='2'/><path d='M5 13l2-8h10l2 8'/><path d='M17 16.5h.01'/>",
    /* kUpload */ "<path d='M12 15V3'/><path d='M7 8l5-5 5 5'/><path d='M4 15v4a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2v-4'/>",
    /* kAlert */ "<circle cx='12' cy='12' r='9'/><path d='M12 7v6'/><path d='M12 16.5h.01'/>",
    /* kMouse */ "<rect x='6' y='3' width='12' height='18' rx='6'/><path d='M12 7v4'/>",
    /* kKeyboard */
    "<rect x='2' y='6' width='20' height='12' rx='2'/><path d='M6 10h.01'/><path d='M10 10h.01'/>"
    "<path d='M14 10h.01'/><path d='M18 10h.01'/><path d='M7 14h10'/>",
    /* kPsCross */ "<path d='M6.5 6.5l11 11'/><path d='M17.5 6.5l-11 11'/>",
    /* kPsCircle */ "<circle cx='12' cy='12' r='6.5'/>",
    /* kPsSquare */ "<rect x='6' y='6' width='12' height='12' rx='1'/>",
    /* kPsTriangle */ "<path d='M12 5.5l7 12.5H5z'/>",
};

struct Picture {
  ImTextureID texture = ImTextureID_Invalid;
  int width = 0;
  int height = 0;
};

// Icons by (icon, pixel size, stroke in hundredths, filled).
std::map<std::tuple<int, int, int, bool>, Picture> g_icons;

struct ArtCache {
  Picture picture;
  std::chrono::steady_clock::time_point drawn_at;
};
ArtCache g_art[int(Art::kCount)];

// Rasterizes SVG text into a width x height picture: scaled by `scale`, moved by (tx, ty) pixels.
Picture Rasterize(std::string svg, int width, int height, float scale, float tx, float ty) {
  Picture picture;
  if (width <= 0 || height <= 0) {
    return picture;
  }
  if (!g_rasterizer) {
    g_rasterizer = nsvgCreateRasterizer();
  }
  NSVGimage* image = nsvgParse(svg.data(), "px", 96.0f);
  if (!image) {
    return picture;
  }
  std::vector<uint8_t> pixels(size_t(width) * size_t(height) * 4);
  nsvgRasterize(g_rasterizer, image, tx, ty, scale, pixels.data(), width, height, width * 4);
  nsvgDelete(image);
  picture.texture = gfx::CreateTexture(pixels.data(), width, height);
  picture.width = width;
  picture.height = height;
  return picture;
}

void Release(Picture& picture) {
  gfx::DestroyTexture(picture.texture);
  picture = {};
}

// Decodes the UTF-8 character at `s`; returns its length.
int NextChar(const char* s, const char* end, unsigned int* c) { return ImTextCharFromUtf8(c, s, end); }

ImDrawList* List() { return ImGui::GetWindowDrawList(); }

}  // namespace

ImU32 Fade(ImU32 color, float alpha) {
  uint32_t a = (color >> IM_COL32_A_SHIFT) & 0xFF;
  a = uint32_t(std::clamp(float(a) * alpha, 0.0f, 255.0f) + 0.5f);
  return (color & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
}

ImU32 Brighten(ImU32 color) {
  auto channel = [&](int shift) {
    uint32_t v = (color >> shift) & 0xFF;
    return std::min<uint32_t>(255, uint32_t(float(v) * 1.15f + 0.5f)) << shift;
  };
  return (color & IM_COL32_A_MASK) | channel(IM_COL32_R_SHIFT) | channel(IM_COL32_G_SHIFT) |
         channel(IM_COL32_B_SHIFT);
}

void SetScale(float pixels_per_dp) { g_scale = pixels_per_dp; }
float Scale() { return g_scale; }

std::string_view Resource(int id) {
  HMODULE module = GetModuleHandleW(nullptr);
  HRSRC info = FindResourceW(module, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10));  // RT_RCDATA
  if (!info) {
    return {};
  }
  HGLOBAL data = LoadResource(module, info);
  const void* bytes = data ? LockResource(data) : nullptr;
  return bytes ? std::string_view(static_cast<const char*>(bytes), SizeofResource(module, info))
               : std::string_view();
}

// --- Type ---

// A font of Windows (C:\Windows\Fonts), mapped into memory for the program's life: only the pages ImGui reads
// are loaded. Empty if it is not there.
std::string_view MapWindowsFont(const wchar_t* name) {
  wchar_t path[MAX_PATH];
  UINT length = GetWindowsDirectoryW(path, MAX_PATH);
  if (length == 0 || length + 8 + wcslen(name) >= MAX_PATH) {
    return {};
  }
  wcscat_s(path, L"\\Fonts\\");
  wcscat_s(path, name);
  HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    return {};
  }
  LARGE_INTEGER size = {};
  HANDLE mapping = GetFileSizeEx(file, &size) ? CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr) : nullptr;
  CloseHandle(file);
  const void* view = mapping ? MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0) : nullptr;
  if (mapping) {
    CloseHandle(mapping);  // the view keeps it
  }
  return view ? std::string_view(static_cast<const char*>(view), size_t(size.QuadPart)) : std::string_view();
}

// Windows' fonts for the scripts Barlow does not have, which the names of the game's languages (game.h, in their
// own words) use: merged after Barlow into each font, in this order.
std::vector<std::string_view> FallbackFonts() {
  bool han = false, kana = false, hangul = false, cyrillic = false;
  bool traditional = false, simplified = false;
  for (const launcher::game::Language& language : launcher::game::kLanguages) {
    traditional = traditional || language.id == 8;
    simplified = simplified || language.id == 10;
    for (const char* s = language.name; *s;) {
      unsigned int c;
      s += NextChar(s, nullptr, &c);
      han = han || (c >= 0x4E00 && c <= 0x9FFF);
      kana = kana || (c >= 0x3040 && c <= 0x30FF);
      hangul = hangul || (c >= 0xAC00 && c <= 0xD7AF) || (c >= 0x1100 && c <= 0x11FF) || (c >= 0x3130 && c <= 0x318F);
      cyrillic = cyrillic || (c >= 0x0400 && c <= 0x04FF);
    }
  }
  std::vector<std::string_view> fonts;
  auto add = [&](std::initializer_list<const wchar_t*> names) {
    for (const wchar_t* name : names) {
      if (std::string_view font = MapWindowsFont(name); !font.empty()) {
        fonts.push_back(font);
        return;
      }
    }
  };
  if (kana || (han && !traditional && !simplified)) add({L"YuGothM.ttc", L"msgothic.ttc"});
  if (han && traditional) add({L"msjh.ttc", L"mingliu.ttc"});
  if (han && simplified) add({L"msyh.ttc", L"simsun.ttc"});
  if (hangul) add({L"malgun.ttf", L"gulim.ttc"});
  if (cyrillic) add({L"segoeui.ttf", L"arial.ttf"});
  return fonts;
}

bool LoadFonts() {
  ImGuiIO& io = ImGui::GetIO();
  constexpr int kIds[int(Font::kCount)] = {IDR_FONT_REGULAR,  IDR_FONT_MEDIUM, IDR_FONT_SEMIBOLD,
                                           IDR_FONT_BOLD,     IDR_FONT_CONDENSED_SEMIBOLD,
                                           IDR_FONT_CONDENSED_BOLD};
  constexpr const char* kNames[int(Font::kCount)] = {"Barlow Regular",          "Barlow Medium",
                                                     "Barlow SemiBold",         "Barlow Bold",
                                                     "Barlow Condensed SemiBold", "Barlow Condensed Bold"};
  for (int i = 0; i < int(Font::kCount); ++i) {
    std::string_view data = Resource(kIds[i]);
    if (data.empty()) {
      return false;
    }
    ImFontConfig config;
    config.FontDataOwnedByAtlas = false;  // the resource stays mapped
    // ImGui sizes a font by its ascent minus its descent, 1.2 em for Barlow (FontPixels). (A negative size
    // would map the em square, but ImGui 1.92 then scales glyph advances by a negative factor.)
    config.SizePixels = 16.0f;
    std::snprintf(config.Name, sizeof(config.Name), "%s", kNames[i]);
    g_fonts[i] = io.Fonts->AddFontFromMemoryTTF(const_cast<char*>(data.data()), int(data.size()), 0.0f, &config);
    if (!g_fonts[i]) {
      return false;
    }
    static const std::vector<std::string_view> fallbacks = FallbackFonts();
    for (std::string_view fallback : fallbacks) {
      ImFontConfig merge;
      merge.MergeMode = true;
      merge.DstFont = g_fonts[i];
      merge.FontDataOwnedByAtlas = false;  // mapped for the program's life
      io.Fonts->AddFontFromMemoryTTF(const_cast<char*>(fallback.data()), int(fallback.size()), 0.0f, &merge);
    }
  }
  return true;
}

float LineHeight(const Type& type) { return Px(type.size) * kBarlowHeight; }

ImVec2 MeasureText(const Type& type, std::string_view text) {
  float size = Px(type.size);
  ImVec2 extent = FontOf(type)->CalcTextSizeA(FontPixels(type), FLT_MAX, 0.0f, text.data(), text.data() + text.size());
  if (type.tracking != 0.0f) {
    int count = 0;
    for (const char* s = text.data(); s < text.data() + text.size();) {
      unsigned int c;
      s += NextChar(s, text.data() + text.size(), &c);
      ++count;
    }
    extent.x += float(count) * type.tracking * size;
  }
  return {extent.x, LineHeight(type)};
}

void DrawLabel(const Type& type, ImVec2 pos, ImU32 color, std::string_view text, float line_height) {
  if (text.empty()) {
    return;
  }
  ImFont* font = FontOf(type);
  float size = Px(type.size);
  float pixels = FontPixels(type);
  // CSS puts the baseline at the top of the line box, plus half the leading, plus the ascent (1 em);
  // ImGui puts it at the top plus its baked (rounded) ascent.
  float box = line_height > 0.0f ? line_height : LineHeight(type);
  float baseline = pos.y + (box - size * kBarlowHeight) * 0.5f + size;
  ImVec2 at = Snap(ImVec2(pos.x, baseline - font->GetFontBaked(pixels)->Ascent));
  const char* begin = text.data();
  const char* end = begin + text.size();
  if (type.tracking == 0.0f) {
    List()->AddText(font, pixels, at, color, begin, end);
    return;
  }
  float spacing = type.tracking * size;
  for (const char* s = begin; s < end;) {
    unsigned int c;
    int length = NextChar(s, end, &c);
    List()->AddText(font, pixels, Snap(at), color, s, s + length);
    at.x += font->CalcTextSizeA(pixels, FLT_MAX, 0.0f, s, s + length).x + spacing;
    s += length;
  }
}

float DrawParagraph(const Type& type, ImVec2 pos, float width, float line_height, ImU32 color,
                      std::string_view text, bool draw, float align) {
  ImFont* font = FontOf(type);
  float size = Px(type.size);
  float step = line_height * size;
  const char* s = text.data();
  const char* end = s + text.size();
  float y = pos.y;
  while (s < end) {
    const char* line_end = font->CalcWordWrapPosition(FontPixels(type), s, end, width);
    if (line_end <= s) {
      line_end = s + 1;
    }
    const char* newline = std::find(s, line_end, '\n');
    if (newline < line_end) {
      line_end = newline;
    }
    if (draw) {
      std::string_view line(s, size_t(line_end - s));
      while (!line.empty() && line.back() == ' ') {
        line.remove_suffix(1);
      }
      float x = pos.x + (align > 0.0f ? (width - MeasureText(type, line).x) * align : 0.0f);
      DrawLabel(type, {x, y}, color, line, step);
    }
    y += step;
    s = line_end;
    if (s < end && *s == '\n') {
      ++s;
    } else {
      while (s < end && *s == ' ') {
        ++s;
      }
    }
  }
  return y - pos.y;
}

std::string Ellipsize(const Type& type, std::string_view text, float width) {
  if (MeasureText(type, text).x <= width) {
    return std::string(text);
  }
  std::string cut(text);
  while (!cut.empty()) {
    // Drop one whole UTF-8 character.
    size_t back = cut.size() - 1;
    while (back > 0 && (static_cast<unsigned char>(cut[back]) & 0xC0) == 0x80) {
      --back;
    }
    cut.resize(back);
    while (!cut.empty() && cut.back() == ' ') {
      cut.pop_back();
    }
    if (MeasureText(type, cut + "\xE2\x80\xA6").x <= width) {
      break;
    }
  }
  return cut + "\xE2\x80\xA6";
}

// --- Pictures ---

void DrawIcon(Icon icon, ImVec2 pos, float size, ImU32 color, float stroke, bool filled) {
  if (icon == Icon::kNone || icon >= Icon::kCount) {
    return;
  }
  int pixels = std::max(1, int(Snap(Px(size))));
  auto key = std::make_tuple(int(icon), pixels, int(stroke * 100.0f + 0.5f), filled);
  auto it = g_icons.find(key);
  if (it == g_icons.end()) {
    char header[256];
    if (filled) {
      std::snprintf(header, sizeof(header),
                    "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#FFFFFF' stroke='none'>");
    } else {
      std::snprintf(header, sizeof(header),
                    "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' stroke='#FFFFFF' "
                    "stroke-width='%g' stroke-linecap='round' stroke-linejoin='round'>",
                    double(stroke));
    }
    std::string svg = std::string(header) + kIconBodies[int(icon)] + "</svg>";
    it = g_icons.emplace(key, Rasterize(std::move(svg), pixels, pixels, float(pixels) / 24.0f, 0, 0)).first;
  }
  if (it->second.texture == ImTextureID_Invalid) {
    return;
  }
  ImVec2 at = Snap(pos);
  List()->AddImage(it->second.texture, at, ImVec2(at.x + float(pixels), at.y + float(pixels)), ImVec2(0, 0),
                   ImVec2(1, 1), color);
}

void DrawArt(Art art, const ImRect& rect, ImVec2 anchor, float rounding, ImDrawFlags corners) {
  constexpr int kIds[int(Art::kCount)] = {IDR_ART_HERO, IDR_ART_INSTALL};
  constexpr float kViewBox[int(Art::kCount)][2] = {{1000.0f, 480.0f}, {440.0f, 760.0f}};
  int width = int(Snap(rect.GetWidth()));
  int height = int(Snap(rect.GetHeight()));
  if (width <= 0 || height <= 0) {
    return;
  }
  ArtCache& cache = g_art[int(art)];
  auto now = std::chrono::steady_clock::now();
  // While the window is being resized, the last picture stretches; it is drawn again at most 10 times a
  // second.
  if ((cache.picture.width != width || cache.picture.height != height) &&
      (cache.picture.texture == ImTextureID_Invalid || now - cache.drawn_at > std::chrono::milliseconds(100))) {
    Release(cache.picture);
    float vw = kViewBox[int(art)][0];
    float vh = kViewBox[int(art)][1];
    float scale = std::max(float(width) / vw, float(height) / vh);
    float tx = (float(width) - vw * scale) * anchor.x;
    float ty = (float(height) - vh * scale) * anchor.y;
    cache.picture = Rasterize(std::string(Resource(kIds[int(art)])), width, height, scale, tx, ty);
    cache.drawn_at = now;
  }
  if (cache.picture.texture == ImTextureID_Invalid) {
    return;
  }
  ImVec2 a = Snap(rect.Min);
  ImVec2 b = Snap(rect.Max);
  if (rounding > 0.0f) {
    List()->AddImageRounded(cache.picture.texture, a, b, ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE,
                            Px(rounding), corners);
  } else {
    List()->AddImage(cache.picture.texture, a, b);
  }
}

void ReleasePictures() {
  for (auto& [key, picture] : g_icons) {
    Release(picture);
  }
  g_icons.clear();
  for (ArtCache& art : g_art) {
    Release(art.picture);
  }
}

// --- Items ---

ItemState Item(ImGuiID id, const ImRect& rect, bool enabled) {
  ItemState state;
  if (!enabled) {
    ImGui::ItemAdd(rect, 0);
    return state;
  }
  if (g_capture_first && !g_first_item) {
    g_first_item = id;
  }
  if (!ImGui::ItemAdd(rect, id)) {
    return state;
  }
  state.pressed = ImGui::ButtonBehavior(rect, id, &state.hovered, &state.held);
  ImGuiContext& g = *GImGui;
  state.focused = g.NavId == id && g.NavCursorVisible && g.NavWindow &&
                  (g.NavWindow == ImGui::GetCurrentWindow() || g.NavWindow->RootWindow == ImGui::GetCurrentWindow()->RootWindow);
  if (state.hovered) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  }
  return state;
}

ItemState Item(const char* id, const ImRect& rect, bool enabled) { return Item(ImGui::GetID(id), rect, enabled); }

bool AnyGamepadButtonDown() {
  for (int key = ImGuiKey_GamepadStart; key <= ImGuiKey_GamepadR3; ++key) {
    if (ImGui::IsKeyDown(ImGuiKey(key))) {
      return true;
    }
  }
  return false;
}

void CaptureFirstItem() {
  g_capture_first = true;
  g_first_item = 0;
}

ImGuiID CapturedFirstItem() {
  g_capture_first = false;
  return g_first_item;
}

void FocusRing(const ImRect& rect, float rounding) {
  float offset = Px(3.0f);
  List()->AddRect(Snap(ImVec2(rect.Min.x - offset, rect.Min.y - offset)),
                  Snap(ImVec2(rect.Max.x + offset, rect.Max.y + offset)), col::kAccent, Px(rounding + 2.0f), 0,
                  Px(2.0f));
}

// --- Shapes ---

void FillRect(const ImRect& rect, ImU32 color, float rounding_dp, ImDrawFlags corners) {
  List()->AddRectFilled(Snap(rect.Min), Snap(rect.Max), color, Px(rounding_dp), corners);
}

void StrokeRect(const ImRect& rect, ImU32 color, float rounding_dp, float thickness_dp) {
  // ImGui strokes on the rectangle's edge: inset by half the thickness, as a CSS border is drawn inside.
  float t = std::max(1.0f, Snap(Px(thickness_dp)));
  ImVec2 a = Snap(rect.Min);
  ImVec2 b = Snap(rect.Max);
  float h = t * 0.5f - 0.5f;
  List()->AddRect(ImVec2(a.x + h, a.y + h), ImVec2(b.x - h - 1.0f, b.y - h - 1.0f), color,
                  std::max(0.0f, Px(rounding_dp) - h), 0, t);
}

void DashedRect(const ImRect& rect, ImU32 color, float rounding_dp, float dash_dp, float gap_dp) {
  ImDrawList* list = List();
  ImVec2 a = Snap(rect.Min);
  ImVec2 b = Snap(rect.Max);
  a.x += 0.5f;
  a.y += 0.5f;
  b.x -= 0.5f;
  b.y -= 0.5f;
  list->PathRect(a, b, Px(rounding_dp));
  std::vector<ImVec2> points(list->_Path.begin(), list->_Path.end());
  list->PathClear();
  if (points.size() < 2) {
    return;
  }
  points.push_back(points.front());
  float dash = Px(dash_dp);
  float gap = Px(gap_dp);
  float period = dash + gap;
  float travelled = 0.0f;
  float thickness = std::max(1.0f, Snap(Px(1.0f)));
  for (size_t i = 0; i + 1 < points.size(); ++i) {
    ImVec2 p = points[i];
    ImVec2 q = points[i + 1];
    float length = std::sqrt((q.x - p.x) * (q.x - p.x) + (q.y - p.y) * (q.y - p.y));
    float t = 0.0f;
    while (t < length) {
      float phase = std::fmod(travelled + t, period);
      float run = phase < dash ? std::min(dash - phase, length - t) : std::min(period - phase, length - t);
      if (phase < dash) {
        ImVec2 from(p.x + (q.x - p.x) * t / length, p.y + (q.y - p.y) * t / length);
        ImVec2 to(p.x + (q.x - p.x) * (t + run) / length, p.y + (q.y - p.y) * (t + run) / length);
        list->AddLine(from, to, color, thickness);
      }
      t += std::max(run, 0.01f);
    }
    travelled += length;
  }
}

// --- Widgets ---

namespace {

struct ButtonColors {
  ImU32 background;
  ImU32 border;  // 0: none
  ImU32 text;
  ImU32 icon;
};

ButtonColors ColorsOf(ButtonKind kind) {
  switch (kind) {
    case ButtonKind::kPrimary:
      return {col::kAccent, 0, col::kOnAccent, col::kOnAccent};
    case ButtonKind::kHero:
      return {col::kHeroButton, col::kHeroButtonBorder, col::kText, col::kTextSecondary};
    case ButtonKind::kOutline:
      return {col::kCard, col::kOutline, col::kText, col::kTextMuted};
    case ButtonKind::kGhost:
      return {0, col::kOutline, col::kText, col::kTextMuted};
    case ButtonKind::kLight:
      return {col::kText, 0, col::kBackground, col::kBackground};
    case ButtonKind::kDonate:
      return {col::kDonate, 0, col::kOnDonate, col::kOnDonate};
    case ButtonKind::kDisabled:
      return {col::kTrack, 0, col::kLabel, col::kLabel};
  }
  return {};
}

}  // namespace

float ButtonWidth(const ButtonSpec& spec, std::string_view label) {
  float width = Px(spec.padding) * 2.0f + MeasureText(spec.type, label).x;
  if (spec.icon != Icon::kNone) {
    width += Px(spec.icon_size) + (label.empty() ? 0.0f : Px(spec.gap));
  }
  return width;
}

bool Button(const char* id, ImVec2 pos, const ButtonSpec& spec, std::string_view label, bool enabled, float width) {
  if (width <= 0.0f) {
    width = ButtonWidth(spec, label);
  }
  ImRect rect(pos, ImVec2(pos.x + width, pos.y + Px(spec.height)));
  ItemState state = Item(id, rect, enabled && spec.kind != ButtonKind::kDisabled);
  ButtonColors c = ColorsOf(enabled ? spec.kind : ButtonKind::kDisabled);
  if (state.hovered) {
    c = {Brighten(c.background), c.border ? Brighten(c.border) : 0, Brighten(c.text), Brighten(c.icon)};
  }
  if (c.background) {
    FillRect(rect, c.background, spec.radius);
  }
  if (c.border) {
    StrokeRect(rect, c.border, spec.radius);
  }
  float content = MeasureText(spec.type, label).x;
  if (spec.icon != Icon::kNone) {
    content += Px(spec.icon_size) + (label.empty() ? 0.0f : Px(spec.gap));
  }
  float x = rect.Min.x + (width - content) * 0.5f;
  float icon_y = rect.Min.y + (rect.GetHeight() - Px(spec.icon_size)) * 0.5f;
  if (spec.icon != Icon::kNone && !spec.icon_after) {
    DrawIcon(spec.icon, ImVec2(x, icon_y), spec.icon_size, c.icon, spec.icon_stroke, spec.icon_filled);
    x += Px(spec.icon_size) + Px(spec.gap);
  }
  DrawLabel(spec.type, ImVec2(x, rect.Min.y), c.text, label, rect.GetHeight());
  if (spec.icon != Icon::kNone && spec.icon_after) {
    x += MeasureText(spec.type, label).x + Px(spec.gap);
    DrawIcon(spec.icon, ImVec2(x, icon_y), spec.icon_size, c.icon, spec.icon_stroke, spec.icon_filled);
  }
  if (state.focused) {
    FocusRing(rect, spec.radius);
  }
  return state.pressed;
}

namespace {

constexpr Type kSegmentType{Font::kSemiBold, 14};

float SegmentWidth(const SegmentedSpec& spec, const std::string& label) {
  return std::max(Px(spec.min_width), MeasureText(kSegmentType, label).x + Px(spec.padding) * 2.0f);
}

}  // namespace

ImVec2 SegmentedSize(const SegmentedSpec& spec, std::span<const std::string> labels) {
  float width = Px(4.0f) * 2.0f + Px(4.0f) * float(std::max<size_t>(labels.size(), 1) - 1);
  for (const std::string& label : labels) {
    width += SegmentWidth(spec, label);
  }
  return {width, Px(spec.height) + Px(4.0f) * 2.0f};
}

bool Segmented(const char* id, ImVec2 pos, const SegmentedSpec& spec, std::span<const std::string> labels,
               int* selected, bool enabled) {
  ImVec2 size = SegmentedSize(spec, labels);
  FillRect(ImRect(pos, ImVec2(pos.x + size.x, pos.y + size.y)), enabled ? spec.background : Fade(spec.background, 0.6f),
           10.0f);
  bool changed = false;
  ImGui::PushID(id);
  float x = pos.x + Px(4.0f);
  for (int i = 0; i < int(labels.size()); ++i) {
    float width = SegmentWidth(spec, labels[i]);
    ImRect rect(ImVec2(x, pos.y + Px(4.0f)), ImVec2(x + width, pos.y + Px(4.0f) + Px(spec.height)));
    ItemState state = Item(ImGui::GetID(i), rect, enabled);
    if (state.pressed && *selected != i) {
      *selected = i;
      changed = true;
    }
    bool on = *selected == i;
    ImU32 background = on ? col::kAccent : 0;
    ImU32 text = on ? col::kOnAccent : col::kTextSoft;
    if (state.hovered) {
      background = background ? Brighten(background) : 0;
      text = Brighten(text);
    }
    if (!enabled) {
      background = background ? col::kDashed : 0;
      text = Fade(on ? col::kTextSoft : col::kTextDim, 0.8f);
    }
    if (background) {
      FillRect(rect, background, 7.0f);
    }
    float text_width = MeasureText(kSegmentType, labels[i]).x;
    DrawLabel(kSegmentType, ImVec2(rect.Min.x + (width - text_width) * 0.5f, rect.Min.y), text, labels[i],
             rect.GetHeight());
    if (state.focused) {
      FocusRing(rect, 7.0f);
    }
    x += width + Px(4.0f);
  }
  ImGui::PopID();
  return changed;
}

bool Toggle(const char* id, ImVec2 pos, bool* value, bool enabled) {
  ImRect rect(pos, ImVec2(pos.x + Px(52.0f), pos.y + Px(30.0f)));
  ItemState state = Item(id, rect, enabled);
  if (state.pressed) {
    *value = !*value;
  }
  ImU32 track = *value ? col::kAccent : col::kOutline;
  ImU32 knob = *value ? col::kOnAccent : col::kTextMuted;
  if (state.hovered) {
    track = Brighten(track);
  }
  if (!enabled) {
    track = Fade(track, 0.45f);
    knob = Fade(knob, 0.6f);
  }
  FillRect(rect, track, 15.0f);
  float knob_x = *value ? rect.Max.x - Px(27.0f) : rect.Min.x + Px(3.0f);
  FillRect(ImRect(ImVec2(knob_x, rect.Min.y + Px(3.0f)), ImVec2(knob_x + Px(24.0f), rect.Min.y + Px(27.0f))), knob,
           12.0f);
  if (state.focused) {
    FocusRing(rect, 15.0f);
  }
  return state.pressed;
}

bool Combo(const char* id, const ImRect& rect, std::span<const std::string> items, int* selected, bool enabled) {
  constexpr Type kType{Font::kMedium, 14};
  ImGuiID item_id = ImGui::GetID(id);
  ItemState state = Item(item_id, rect, enabled);
  ImU32 background = enabled ? col::kField : col::kFieldDisabled;
  ImU32 border = enabled ? col::kFieldBorder : col::kSidebarBorder;
  ImU32 text = enabled ? col::kText : col::kLabel;
  ImU32 chevron = enabled ? col::kTextMuted : col::kTextDim;
  if (state.hovered) {
    background = Brighten(background);
    border = Brighten(border);
    text = Brighten(text);
  }
  FillRect(rect, background, 8.0f);
  StrokeRect(rect, border, 8.0f);
  std::string current = *selected >= 0 && *selected < int(items.size()) ? items[*selected] : std::string();
  float text_width = rect.GetWidth() - Px(14.0f) - Px(40.0f);
  DrawLabel(kType, ImVec2(rect.Min.x + Px(14.0f), rect.Min.y), text, Ellipsize(kType, current, text_width),
           rect.GetHeight());
  DrawIcon(Icon::kChevronDown, ImVec2(rect.Max.x - Px(30.0f), rect.Min.y + (rect.GetHeight() - Px(16.0f)) * 0.5f),
           16.0f, chevron, 2.0f);
  if (state.focused) {
    FocusRing(rect, 8.0f);
  }

  char popup[64];
  std::snprintf(popup, sizeof(popup), "##popup%08X", item_id);
  if (state.pressed) {
    ImGui::OpenPopup(popup);
  }
  bool changed = false;
  float item_height = Px(40.0f);
  float padding = Px(4.0f);
  float wanted = item_height * float(items.size()) + padding * 2.0f;
  float visible = std::min(wanted, item_height * 8.0f + padding * 2.0f);
  ImVec2 at(rect.Min.x, rect.Max.y + Px(4.0f));
  ImVec2 viewport_max = ImGui::GetMainViewport()->WorkPos + ImGui::GetMainViewport()->WorkSize;
  if (at.y + visible > viewport_max.y && rect.Min.y - Px(4.0f) - visible > 0.0f) {
    at.y = rect.Min.y - Px(4.0f) - visible;
  }
  ImGui::SetNextWindowPos(at);
  ImGui::SetNextWindowSize(ImVec2(rect.GetWidth(), visible));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padding, padding));
  ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, Px(8.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
  ImGui::PushStyleColor(ImGuiCol_PopupBg, ImGui::ColorConvertU32ToFloat4(col::kCardRaised));
  ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertU32ToFloat4(col::kFieldBorder));
  if (ImGui::BeginPopup(popup, ImGuiWindowFlags_NoMove)) {
    float width = ImGui::GetContentRegionAvail().x;
    for (int i = 0; i < int(items.size()); ++i) {
      ImVec2 p = ImGui::GetCursorScreenPos();
      ImRect row(p, ImVec2(p.x + width, p.y + item_height));
      ImGui::PushID(i);
      ItemState row_state = Item("##item", row, true);
      ImGui::PopID();
      bool on = i == *selected;
      if (row_state.hovered || row_state.focused) {
        FillRect(row, col::kTrack, 6.0f);
      }
      ImU32 row_text = on ? col::kAccentLight : col::kText;
      DrawLabel(kType, ImVec2(row.Min.x + Px(10.0f), row.Min.y), row_text,
               Ellipsize(kType, items[i], width - Px(44.0f)), row.GetHeight());
      if (on) {
        DrawIcon(Icon::kCheck, ImVec2(row.Max.x - Px(28.0f), row.Min.y + (row.GetHeight() - Px(16.0f)) * 0.5f), 16.0f,
                 col::kAccent, 2.4f);
        ImGui::SetItemDefaultFocus();
        if (ImGui::IsWindowAppearing()) {
          ImGui::SetScrollHereY(0.5f);
        }
      }
      ImGui::Dummy(ImVec2(width, item_height));
      if (row_state.pressed) {
        changed = *selected != i;
        *selected = i;
        ImGui::CloseCurrentPopup();
      }
    }
    ImGui::EndPopup();
  }
  ImGui::PopStyleColor(2);
  ImGui::PopStyleVar(4);
  return changed;
}

bool Slider(const char* id, const ImRect& rect, float* value, float min, float max, float step, bool enabled) {
  ImGuiID item_id = ImGui::GetID(id);
  ItemState state = Item(item_id, rect, enabled);
  float before = *value;
  ImGuiContext& g = *GImGui;
  // Left and right step the value instead of moving the focus.
  if (enabled && g.NavId == item_id && g.NavMoveSubmitted &&
      (g.NavMoveDir == ImGuiDir_Left || g.NavMoveDir == ImGuiDir_Right)) {
    *value = std::clamp(*value, min, max) + (g.NavMoveDir == ImGuiDir_Left ? -step : step);
    ImGui::NavMoveRequestCancel();
  }
  float knob = Px(18.0f);
  float left = rect.Min.x + knob * 0.5f;
  float right = rect.Max.x - knob * 0.5f;
  if (state.held && right > left) {
    float t = std::clamp((ImGui::GetIO().MousePos.x - left) / (right - left), 0.0f, 1.0f);
    *value = min + t * (max - min);
    if (step > 0.0f) {
      *value = min + std::round((*value - min) / step) * step;
    }
  }
  // A value outside the range (an options file edited by hand) stays as it is until the slider moves.
  if (*value != before) {
    *value = std::clamp(*value, min, max);
  }
  float t = max > min ? std::clamp((*value - min) / (max - min), 0.0f, 1.0f) : 0.0f;
  float cy = rect.GetCenter().y;
  ImRect track(ImVec2(left, cy - Px(3.0f)), ImVec2(right, cy + Px(3.0f)));
  float alpha = enabled ? 1.0f : 0.45f;
  FillRect(track, Fade(col::kTrack, alpha), 3.0f);
  FillRect(ImRect(track.Min, ImVec2(left + (right - left) * t, track.Max.y)), Fade(col::kAccent, alpha), 3.0f);
  ImVec2 center(left + (right - left) * t, cy);
  ImU32 knob_color = state.hovered || state.held ? Brighten(col::kText) : col::kText;
  List()->AddCircleFilled(center, knob * 0.5f, Fade(knob_color, alpha), 24);
  if (state.focused) {
    FocusRing(ImRect(ImVec2(rect.Min.x, cy - knob * 0.5f), ImVec2(rect.Max.x, cy + knob * 0.5f)), 9.0f);
  }
  return *value != before;
}

bool Link(const char* id, ImVec2 pos, std::string_view label, float size) {
  Type type{Font::kSemiBold, size};
  ImVec2 text = MeasureText(type, label);
  float arrow = size * 0.95f;
  ImRect rect(pos, ImVec2(pos.x + text.x + Px(4.0f) + Px(arrow), pos.y + text.y));
  ItemState state = Item(id, rect, true);
  ImU32 color = state.hovered ? col::kAccentLight : col::kAccent;
  DrawLabel(type, pos, color, label);
  DrawIcon(Icon::kArrowRight, ImVec2(pos.x + text.x + Px(4.0f), pos.y + (text.y - Px(arrow)) * 0.5f + Px(0.5f)),
           arrow, color, 2.2f);
  if (state.focused) {
    FocusRing(rect, 4.0f);
  }
  return state.pressed;
}

ImVec2 BadgeSize(std::string_view label, float size) {
  Type type{Font::kBold, size, 0.06f};
  ImVec2 text = MeasureText(type, label);
  return {text.x + Px(16.0f), text.y + Px(6.0f)};
}

void Badge(ImVec2 pos, std::string_view label, ImU32 background, ImU32 color, float size) {
  Type type{Font::kBold, size, 0.06f};
  ImVec2 badge = BadgeSize(label, size);
  FillRect(ImRect(pos, ImVec2(pos.x + badge.x, pos.y + badge.y)), background, 5.0f);
  DrawLabel(type, ImVec2(pos.x + Px(8.0f), pos.y + Px(3.0f)), color, label);
}

void Progress(const ImRect& rect, float fraction, ImU32 track, ImU32 fill) {
  float radius = rect.GetHeight() * 0.5f / Scale();
  FillRect(rect, track, radius);
  fraction = std::clamp(fraction, 0.0f, 1.0f);
  if (fraction > 0.0f) {
    // overflow: hidden with a rounded track: the fill's left corners are rounded, its right edge straight.
    ImRect bar(rect.Min, ImVec2(rect.Min.x + rect.GetWidth() * fraction, rect.Max.y));
    FillRect(bar, fill, radius, fraction >= 0.999f ? 0 : ImDrawFlags_RoundCornersLeft);
  }
}

void BeginScroll(const char* id, const ImRect& rect) {
  ImGui::SetCursorScreenPos(rect.Min);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, Px(10.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, Px(5.0f));
  ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImGui::ColorConvertU32ToFloat4(col::kOutline));
  ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImGui::ColorConvertU32ToFloat4(col::kDashed));
  ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, ImGui::ColorConvertU32ToFloat4(col::kScrollbarActive));
  ImGui::BeginChild(id, rect.GetSize(), ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoBackground);
}

void EndScroll() {
  ImGui::EndChild();
  ImGui::PopStyleColor(4);
  ImGui::PopStyleVar(4);
}

}  // namespace ui
