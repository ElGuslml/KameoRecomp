// The Gamepad page: the controllers SDL sees, and which of their buttons presses each control of the
// original Xbox 360 controller. That is the SDK's gamepad_remap (recomp change 36,
// thirdparty/rexglue-sdk/src/input/sdl/sdl_input_driver.cpp): one layout per type of controller, kept in
// the options file, applied by the game to every controller of that type. The names, the standard layout
// and the order sections apply in are the SDK's, repeated here.

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_stdinc.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include "app.h"
#include "pages.h"

namespace launcher {

using namespace ui;

namespace {

// --- gamepad_remap, as the SDK reads it ---

constexpr int kInputLeftTrigger = 32;
constexpr int kInputRightTrigger = 33;
constexpr uint64_t Bit(int input) { return uint64_t(1) << input; }

constexpr int kControls = 13;
enum Control : int { kA, kB, kX, kY, kLB, kRB, kLT, kRT, kLS, kRS, kStart, kBack, kGuide };
constexpr const char* kControlNames[kControls] = {"a",  "b",  "x",  "y",     "lb",   "rb",   "lt",
                                                  "rt", "ls", "rs", "start", "back", "guide"};

struct InputName {
  const char* name;
  int input;
};
constexpr InputName kInputNames[] = {
    {"south", SDL_GAMEPAD_BUTTON_SOUTH},
    {"east", SDL_GAMEPAD_BUTTON_EAST},
    {"west", SDL_GAMEPAD_BUTTON_WEST},
    {"north", SDL_GAMEPAD_BUTTON_NORTH},
    {"back", SDL_GAMEPAD_BUTTON_BACK},
    {"guide", SDL_GAMEPAD_BUTTON_GUIDE},
    {"start", SDL_GAMEPAD_BUTTON_START},
    {"ls", SDL_GAMEPAD_BUTTON_LEFT_STICK},
    {"rs", SDL_GAMEPAD_BUTTON_RIGHT_STICK},
    {"lb", SDL_GAMEPAD_BUTTON_LEFT_SHOULDER},
    {"rb", SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER},
    {"dpup", SDL_GAMEPAD_BUTTON_DPAD_UP},
    {"dpdown", SDL_GAMEPAD_BUTTON_DPAD_DOWN},
    {"dpleft", SDL_GAMEPAD_BUTTON_DPAD_LEFT},
    {"dpright", SDL_GAMEPAD_BUTTON_DPAD_RIGHT},
    {"misc1", SDL_GAMEPAD_BUTTON_MISC1},
    {"paddle1", SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1},
    {"paddle2", SDL_GAMEPAD_BUTTON_LEFT_PADDLE1},
    {"paddle3", SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2},
    {"paddle4", SDL_GAMEPAD_BUTTON_LEFT_PADDLE2},
    {"touchpad", SDL_GAMEPAD_BUTTON_TOUCHPAD},
    {"misc2", SDL_GAMEPAD_BUTTON_MISC2},
    {"misc3", SDL_GAMEPAD_BUTTON_MISC3},
    {"misc4", SDL_GAMEPAD_BUTTON_MISC4},
    {"misc5", SDL_GAMEPAD_BUTTON_MISC5},
    {"misc6", SDL_GAMEPAD_BUTTON_MISC6},
    {"lt", kInputLeftTrigger},
    {"rt", kInputRightTrigger},
};

using Sources = std::array<uint64_t, kControls>;

// The standard layout (the SDK's kDefaultSources).
constexpr Sources kDefaultSources = {
    Bit(SDL_GAMEPAD_BUTTON_SOUTH) | Bit(SDL_GAMEPAD_BUTTON_LEFT_PADDLE2),
    Bit(SDL_GAMEPAD_BUTTON_EAST) | Bit(SDL_GAMEPAD_BUTTON_LEFT_PADDLE1),
    Bit(SDL_GAMEPAD_BUTTON_WEST) | Bit(SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2),
    Bit(SDL_GAMEPAD_BUTTON_NORTH) | Bit(SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1),
    Bit(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER),
    Bit(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER),
    Bit(kInputLeftTrigger),
    Bit(kInputRightTrigger),
    Bit(SDL_GAMEPAD_BUTTON_LEFT_STICK),
    Bit(SDL_GAMEPAD_BUTTON_RIGHT_STICK),
    Bit(SDL_GAMEPAD_BUTTON_START),
    Bit(SDL_GAMEPAD_BUTTON_BACK),
    Bit(SDL_GAMEPAD_BUTTON_GUIDE) | Bit(SDL_GAMEPAD_BUTTON_MISC1) | Bit(SDL_GAMEPAD_BUTTON_TOUCHPAD),
};

std::string_view Trim(std::string_view s) {
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
  return s;
}

template <typename F>
void ForEachPiece(std::string_view s, char separator, F f) {
  while (true) {
    size_t end = s.find(separator);
    f(Trim(s.substr(0, end)));
    if (end == std::string_view::npos) return;
    s.remove_prefix(end + 1);
  }
}

bool EqualsCI(std::string_view a, std::string_view b) {
  return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
           return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
         });
}

// A section's type ("*", "ps5"...), or empty for a piece that is not a section.
std::string_view SectionType(std::string_view section) {
  size_t colon = section.find(':');
  return colon == std::string_view::npos ? std::string_view() : Trim(section.substr(0, colon));
}

// The sources of each control for `type`: the standard layout, then the '*' sections, then (with
// `with_type`) the sections of `type`.
Sources SourcesFor(std::string_view text, std::string_view type, bool with_type) {
  Sources sources = kDefaultSources;
  for (int pass = 0; pass < (with_type ? 2 : 1); ++pass) {
    ForEachPiece(text, ';', [&](std::string_view section) {
      std::string_view section_type = SectionType(section);
      if (section_type.empty() || !(pass == 0 ? section_type == "*" : EqualsCI(section_type, type))) {
        return;
      }
      ForEachPiece(section.substr(section.find(':') + 1), ',', [&](std::string_view entry) {
        size_t equals = entry.find('=');
        if (equals == std::string_view::npos) return;
        std::string_view control = Trim(entry.substr(0, equals));
        auto c = std::find_if(std::begin(kControlNames), std::end(kControlNames),
                              [&](const char* name) { return EqualsCI(control, name); });
        if (c == std::end(kControlNames)) return;
        uint64_t mask = 0;
        ForEachPiece(entry.substr(equals + 1), '|', [&](std::string_view input) {
          auto i = std::find_if(std::begin(kInputNames), std::end(kInputNames),
                                [&](const InputName& n) { return EqualsCI(input, n.name); });
          if (i != std::end(kInputNames)) mask |= Bit(i->input);
        });
        sources[size_t(c - std::begin(kControlNames))] = mask;
      });
    });
  }
  return sources;
}

// gamepad_remap with `type`'s sections replaced by one holding what `sources` changes from the rest.
std::string WithLayout(std::string_view text, std::string_view type, const Sources& sources) {
  Sources base = SourcesFor(text, type, false);
  std::string entries;
  for (int c = 0; c < kControls; ++c) {
    if (sources[size_t(c)] == base[size_t(c)]) continue;
    std::string inputs;
    for (const InputName& n : kInputNames) {
      if (sources[size_t(c)] & Bit(n.input)) inputs += (inputs.empty() ? "" : "|") + std::string(n.name);
    }
    entries += (entries.empty() ? "" : ",") + std::string(kControlNames[c]) + "=" + inputs;
  }
  std::string out;
  ForEachPiece(text, ';', [&](std::string_view section) {
    if (section.empty() || EqualsCI(SectionType(section), type)) return;
    out += (out.empty() ? "" : ";") + std::string(section);
  });
  if (!entries.empty()) {
    out += (out.empty() ? "" : ";") + std::string(type) + ":" + entries;
  }
  return out;
}

// The input a control shows: the first of its sources in the names' order (A shows South, not the Elite's
// paddle that also presses it), or -1.
int PrimaryInput(uint64_t sources) {
  for (const InputName& n : kInputNames) {
    if (sources & Bit(n.input)) return n.input;
  }
  return -1;
}

// --- The controllers ---

struct Pad {
  std::string name;
  SDL_GamepadType type = SDL_GAMEPAD_TYPE_UNKNOWN;
};

std::vector<Pad> ConnectedPads() {
  std::vector<Pad> pads;
  int count = 0;
  SDL_JoystickID* ids = SDL_GetGamepads(&count);
  for (int i = 0; i < count; ++i) {
    Pad pad;
    const char* name = SDL_GetGamepadNameForID(ids[i]);
    pad.name = name && *name ? name : "";
    pad.type = SDL_GetGamepadTypeForID(ids[i]);
    pads.push_back(std::move(pad));
  }
  SDL_free(ids);
  return pads;
}

enum class Family { kXbox, kPlayStation, kNintendo };

Family FamilyOf(SDL_GamepadType type) {
  switch (type) {
    case SDL_GAMEPAD_TYPE_PS3:
    case SDL_GAMEPAD_TYPE_PS4:
    case SDL_GAMEPAD_TYPE_PS5:
      return Family::kPlayStation;
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
      return Family::kNintendo;
    default:
      return Family::kXbox;
  }
}

const char* TypeName(SDL_GamepadType type, const Texts& t) {
  switch (type) {
    case SDL_GAMEPAD_TYPE_XBOX360:
      return "Xbox 360";
    case SDL_GAMEPAD_TYPE_XBOXONE:
      return "Xbox One / Series";
    case SDL_GAMEPAD_TYPE_PS3:
      return "PlayStation 3";
    case SDL_GAMEPAD_TYPE_PS4:
      return "PlayStation 4";
    case SDL_GAMEPAD_TYPE_PS5:
      return "PlayStation 5";
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
      return "Nintendo Switch Pro";
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
      return "Joy-Con";
    default:
      return t.controller;
  }
}

// How a physical input is labeled on a controller of `type`: text, or an icon (PlayStation symbols, the
// D-pad's arrows).
struct Label {
  std::string text;
  Icon icon = Icon::kNone;
  ImU32 color = col::kText;
};

Label LabelOf(int input, SDL_GamepadType type) {
  Family family = FamilyOf(type);
  bool ps4 = type == SDL_GAMEPAD_TYPE_PS4;
  bool ps5 = type == SDL_GAMEPAD_TYPE_PS5;
  bool one = type == SDL_GAMEPAD_TYPE_XBOXONE;
  // The face buttons by position: South, East, West, North.
  if (input >= 0 && input <= SDL_GAMEPAD_BUTTON_NORTH) {
    if (family == Family::kPlayStation) {
      constexpr Icon kIcons[] = {Icon::kPsCross, Icon::kPsCircle, Icon::kPsSquare, Icon::kPsTriangle};
      constexpr ImU32 kColors[] = {Hex(0x7CB2E8), Hex(0xF07B6B), Hex(0xE6A4D7), Hex(0x5BC489)};
      return {"", kIcons[input], kColors[input]};
    }
    if (family == Family::kNintendo) {
      constexpr const char* kNames[] = {"B", "A", "Y", "X"};
      return {kNames[input]};
    }
    constexpr const char* kNames[] = {"A", "B", "X", "Y"};
    constexpr ImU32 kColors[] = {col::kGlyphA, col::kGlyphB, col::kGlyphX, col::kGlyphY};
    return {kNames[input], Icon::kNone, kColors[input]};
  }
  auto pick = [family](const char* xbox, const char* playstation, const char* nintendo) {
    return Label{family == Family::kPlayStation ? playstation : family == Family::kNintendo ? nintendo : xbox};
  };
  switch (input) {
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
      return pick("LB", "L1", "L");
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
      return pick("RB", "R1", "R");
    case kInputLeftTrigger:
      return pick("LT", "L2", "ZL");
    case kInputRightTrigger:
      return pick("RT", "R2", "ZR");
    case SDL_GAMEPAD_BUTTON_LEFT_STICK:
      return pick("LS", "L3", "LS");
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
      return pick("RS", "R3", "RS");
    case SDL_GAMEPAD_BUTTON_START:
      return pick(one ? "Menu" : "Start", ps4 || ps5 ? "Options" : "Start", "+");
    case SDL_GAMEPAD_BUTTON_BACK:
      return pick(one ? "View" : "Back", ps5 ? "Create" : ps4 ? "Share" : "Select", "−");
    case SDL_GAMEPAD_BUTTON_GUIDE:
      return pick("Xbox", "PS", "Home");
    case SDL_GAMEPAD_BUTTON_MISC1:
      return pick("Share", "Mic", "Capture");
    case SDL_GAMEPAD_BUTTON_TOUCHPAD:
      return {"Touchpad"};
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1:
      return {"P1"};
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE1:
      return {"P3"};
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2:
      return {"P2"};
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE2:
      return {"P4"};
    case SDL_GAMEPAD_BUTTON_DPAD_UP:
      return {"", Icon::kArrowUp};
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
      return {"", Icon::kArrowDown};
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
      return {"", Icon::kArrowLeft};
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
      return {"", Icon::kArrowRight};
    default:
      if (input >= SDL_GAMEPAD_BUTTON_MISC2 && input <= SDL_GAMEPAD_BUTTON_MISC6) {
        return {"M" + std::to_string(input - SDL_GAMEPAD_BUTTON_MISC2 + 2)};
      }
      return {"—", Icon::kNone, col::kTextDim};  // no button: the control is off
  }
}

// The page's rows, in the mockup's order: the original buttons, the D-pad (not remappable) before Guide.
constexpr int kRows = 14;
constexpr int kRowControl[kRows] = {kA, kB, kX, kY, kLB, kRB, kLT, kRT, kLS, kRS, kStart, kBack, -1, kGuide};
constexpr const char* kRowGlyphs[kRows] = {"A",  "B",  "X",  "Y",     "LB",   "RB",    "LT",
                                           "RT", "LS", "RS", "START", "BACK", "D-PAD", "GUIDE"};

// gamepad_remap's name for a type of controller (SDL's: "ps5", "xboxone"...).
std::string TypeKey(SDL_GamepadType type) {
  const char* name = SDL_GetGamepadStringForType(type);
  return name ? name : "standard";
}

}  // namespace

bool App::OnPadCapture(const SDL_Event& event) {
  if (pad_listen_ < 0) {
    return false;
  }
  switch (event.type) {
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
      // Only a controller of the type being edited.
      if (SDL_GetGamepadTypeForID(event.gbutton.which) == pad_type_) {
        AssignPad(event.gbutton.button);
      }
      return true;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
      if ((event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || event.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) &&
          event.gaxis.value > 16000 && SDL_GetGamepadTypeForID(event.gaxis.which) == pad_type_) {
        AssignPad(event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER ? kInputLeftTrigger : kInputRightTrigger);
      }
      return true;
    case SDL_EVENT_KEY_DOWN:
      if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
        StopListening();
      }
      return true;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
      StopListening();  // a click anywhere cancels
      return true;
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
    case SDL_EVENT_KEY_UP:
    case SDL_EVENT_TEXT_INPUT:
    case SDL_EVENT_MOUSE_BUTTON_UP:
      return true;
    default:
      return false;
  }
}

// The input pressed for the control being assigned. A button another control had moves over to it: the two
// swap, so every control stays on some button.
void App::AssignPad(int input) {
  std::string type = TypeKey(pad_type_);
  std::string& remap = settings_.o.gamepad_remap;
  Sources sources = SourcesFor(remap, type, true);
  int control = pad_listen_;
  int old = PrimaryInput(sources[size_t(control)]);
  if (sources[size_t(control)] != Bit(input)) {
    for (int c = 0; c < kControls; ++c) {
      if (c != control && (sources[size_t(c)] & Bit(input))) {
        sources[size_t(c)] = (sources[size_t(c)] & ~Bit(input)) | (old >= 0 && old != input ? Bit(old) : 0);
      }
    }
    sources[size_t(control)] = Bit(input);
    // A control back on its usual button gets all its usual inputs again (A's Elite paddle with South).
    Sources base = SourcesFor(remap, type, false);
    for (int c = 0; c < kControls; ++c) {
      if (PrimaryInput(sources[size_t(c)]) == PrimaryInput(base[size_t(c)])) {
        sources[size_t(c)] = base[size_t(c)];
      }
    }
    remap = WithLayout(remap, type, sources);
    Changed();
  }
  StopListening();
  pad_release_wait_ = true;
}

void App::GamepadPage(const ImRect& rect) {
  ImRect content = PageContent(rect);
  float y = Header(content.Min, t_.controls, t_.nav_gamepad, t_.gamepad_subtitle);
  std::vector<Pad> pads = ConnectedPads();
  if (selected_pad_ >= int(pads.size())) {
    selected_pad_ = 0;
  }
  bool any = !pads.empty();
  SDL_GamepadType type = any ? pads[size_t(selected_pad_)].type : SDL_GAMEPAD_TYPE_XBOX360;
  if (type != pad_type_) {
    if (pad_listen_ >= 0) StopListening();
    pad_type_ = type;
  }
  std::string type_key = TypeKey(type);
  Sources sources = SourcesFor(settings_.o.gamepad_remap, type_key, true);
  {
    // Back to the standard layout, for this type of controller.
    ButtonSpec reset;
    reset.icon = Icon::kReset;
    reset.icon_size = 16;
    float width = ButtonWidth(reset, t_.reset_defaults);
    if (Button("reset", ImVec2(content.Max.x - width, y - Px(44.0f)), reset, t_.reset_defaults, any)) {
      StopListening();
      settings_.o.gamepad_remap = WithLayout(settings_.o.gamepad_remap, type_key,
                                             SourcesFor(settings_.o.gamepad_remap, type_key, false));
      Changed();
    }
  }
  y += Px(20.0f);
  constexpr Type kSection{Font::kBold, 12, 0.1f};

  // The controllers.
  ImRect left(ImVec2(content.Min.x, y), ImVec2(content.Min.x + Px(300.0f), content.Max.y));
  FillRect(left, col::kCard, 12.0f);
  {
    float x = left.Min.x + Px(20.0f);
    float width = left.GetWidth() - Px(40.0f);
    float top = left.Min.y + Px(20.0f);
    DrawLabel(kSection, ImVec2(x, top), col::kLabel, t_.detected);
    top += LineHeight(kSection) + Px(4.0f) + Px(10.0f);
    if (!any) {
      DrawParagraph(Type{Font::kRegular, 14}, ImVec2(x, top), width, 1.4f, col::kTextMuted, t_.no_controllers);
    }
    constexpr Type kName{Font::kSemiBold, 14};
    constexpr Type kKind{Font::kRegular, 12};
    for (int i = 0; i < int(pads.size()); ++i) {
      float height = Px(14.0f) * 2.0f + LineHeight(kName) + Px(3.0f) + LineHeight(kKind);
      ImRect r(ImVec2(x, top), ImVec2(x + width, top + height));
      ImGui::PushID(i);
      ItemState state = Item("pad", r);
      ImGui::PopID();
      bool on = i == selected_pad_;
      if (state.pressed) {
        selected_pad_ = i;
      }
      FillRect(r, state.hovered ? Brighten(on ? col::kSegment : col::kSidebar) : on ? col::kSegment : col::kSidebar,
               10.0f);
      StrokeRect(r, on ? col::kAccent : col::kSidebarBorder, 10.0f, 2.0f);
      DrawIcon(Icon::kGamepad, ImVec2(r.Min.x + Px(14.0f), r.Min.y + (height - Px(28.0f)) * 0.5f), 28.0f,
               on ? col::kAccent : col::kTextDim, 1.6f);
      float text_x = r.Min.x + Px(14.0f) + Px(28.0f) + Px(12.0f);
      float text_width = r.Max.x - Px(14.0f) - text_x;
      const Pad& pad = pads[size_t(i)];
      std::string name = pad.name.empty() ? TypeName(pad.type, t_) : pad.name;
      DrawLabel(kName, ImVec2(text_x, r.Min.y + Px(14.0f)), col::kText, Ellipsize(kName, name, text_width));
      std::string kind = std::string(TypeName(pad.type, t_)) + " · " + t_.connected;
      DrawLabel(kKind, ImVec2(text_x, r.Min.y + Px(14.0f) + LineHeight(kName) + Px(3.0f)), col::kTextMuted,
                Ellipsize(kKind, kind, text_width));
      if (state.focused) {
        FocusRing(r, 10.0f);
      }
      top += height + Px(10.0f);
    }
    // SDL adds and removes controllers by itself; the button reads the list again.
    ButtonSpec find{ButtonKind::kGhost, Type{Font::kSemiBold, 14}, 44, 18, 8, Icon::kRefresh, 16};
    Button("find", ImVec2(x, left.Max.y - Px(20.0f) - Px(44.0f)), find, t_.find_controllers, true, width);
  }

  // Each original button and the chosen controller's button that presses it.
  ImRect right(ImVec2(left.Max.x + Px(20.0f), y), content.Max);
  FillRect(right, col::kCard, 12.0f);
  {
    float x = right.Min.x + Px(24.0f);
    float width = right.GetWidth() - Px(48.0f);
    float top = right.Min.y + Px(20.0f);
    std::string name = !any ? std::string("Xbox 360")
                       : pads[size_t(selected_pad_)].name.empty() ? std::string(TypeName(type, t_))
                                                                  : pads[size_t(selected_pad_)].name;
    // "ORIGINAL BUTTON → CONTROLLER": Barlow has no arrow, an icon draws it.
    std::string upper;
    for (char c : name) upper += (c >= 'a' && c <= 'z') ? char(c - 32) : c;
    constexpr Type kHint{Font::kRegular, 13};
    float hint_width = MeasureText(kHint, t_.mapping_hint).x;
    float title_width = MeasureText(kSection, t_.mapping_title).x;
    float arrow = Px(14.0f);
    DrawLabel(kSection, ImVec2(x, top), col::kLabel, t_.mapping_title);
    DrawIcon(Icon::kArrowRight, ImVec2(x + title_width + Px(4.0f), top + (LineHeight(kSection) - arrow) * 0.5f), 14.0f,
             col::kLabel, 2.2f);
    float name_x = x + title_width + Px(4.0f) + arrow + Px(6.0f);
    DrawLabel(kSection, ImVec2(name_x, top), col::kLabel,
              Ellipsize(kSection, upper, right.Max.x - Px(24.0f) - hint_width - Px(16.0f) - name_x));
    DrawLabel(kHint, ImVec2(right.Max.x - Px(24.0f) - hint_width, top + (LineHeight(kSection) - LineHeight(kHint))),
              col::kTextMuted, t_.mapping_hint);
    top += LineHeight(kSection) + Px(14.0f);

    constexpr ImU32 kGlyphColors[] = {col::kGlyphA, col::kGlyphB, col::kGlyphX, col::kGlyphY};
    constexpr Type kGlyph{Font::kBold, 12};
    constexpr Type kLabel{Font::kRegular, 14};
    constexpr Type kButton{Font::kSemiBold, 14};
    float column_gap = Px(16.0f);
    float column_width = (width - column_gap) * 0.5f;
    float row_height = Px(52.0f);
    for (int row = 0; row < kRows; ++row) {
      float cx = x + float(row % 2) * (column_width + column_gap);
      float cy = top + float(row / 2) * (row_height + Px(8.0f));
      if (cy + row_height > right.Max.y - Px(44.0f)) {
        break;  // a short window: the rest does not fit
      }
      int control = kRowControl[row];
      // The original button.
      float glyph_width = std::max(Px(44.0f), MeasureText(kGlyph, kRowGlyphs[row]).x + Px(16.0f));
      ImRect pill(ImVec2(cx + Px(4.0f), cy + (row_height - Px(28.0f)) * 0.5f),
                  ImVec2(cx + Px(4.0f) + glyph_width, cy + (row_height + Px(28.0f)) * 0.5f));
      FillRect(pill, col::kGlyph, 14.0f);
      DrawLabel(kGlyph, ImVec2(pill.Min.x + (glyph_width - MeasureText(kGlyph, kRowGlyphs[row]).x) * 0.5f, pill.Min.y),
                row < 4 ? kGlyphColors[row] : col::kTextSoft, kRowGlyphs[row], pill.GetHeight());
      // Its name and the button on the controller (the D-pad stays where it is).
      float button_width = std::min(Px(150.0f), column_width * 0.45f);
      ImRect button(ImVec2(cx + column_width - Px(4.0f) - button_width, cy + Px(7.0f)),
                    ImVec2(cx + column_width - Px(4.0f), cy + Px(45.0f)));
      float label_x = pill.Max.x + Px(12.0f);
      DrawParagraph(kLabel, ImVec2(label_x, cy + (row_height - LineHeight(kLabel)) * 0.5f),
                    button.Min.x - Px(12.0f) - label_x, 1.2f, col::kTextSoft, t_.pad_buttons[row]);
      bool remappable = control >= 0 && any;
      ImGui::PushID(row);
      ItemState state = Item("button", button, remappable);
      ImGui::PopID();
      bool listening = remappable && pad_listen_ == control;
      if (state.pressed) {
        StopListening();
        if (!listening) {
          pad_listen_ = control;
        }
        listening = !listening;
      }
      if (listening) {
        FillRect(button, col::kAccentTint, 8.0f);
        StrokeRect(button, col::kAccent, 8.0f, 2.0f);
        DrawLabel(kButton, ImVec2(button.Min.x + Px(12.0f), button.Min.y), col::kAccentLight,
                  Ellipsize(kButton, t_.press_button, button_width - Px(24.0f)), button.GetHeight());
      } else {
        ImU32 background = remappable ? col::kField : col::kFieldDisabled;
        FillRect(button, state.hovered ? Brighten(background) : background, 8.0f);
        StrokeRect(button, remappable ? col::kFieldBorder : col::kSidebarBorder, 8.0f);
        Label label = control >= 0 ? LabelOf(PrimaryInput(sources[size_t(control)]), type)
                                   : Label{t_.pad_buttons[12], Icon::kNone, col::kLabel};
        float text_x = button.Min.x + Px(12.0f);
        if (!label.text.empty()) {
          DrawLabel(kButton, ImVec2(text_x, button.Min.y), remappable ? label.color : col::kLabel,
                    Ellipsize(kButton, label.text, button_width - Px(24.0f)), button.GetHeight());
          text_x += MeasureText(kButton, label.text).x + Px(4.0f);
        }
        if (label.icon != Icon::kNone) {
          DrawIcon(label.icon, ImVec2(text_x, button.Min.y + (button.GetHeight() - Px(18.0f)) * 0.5f), 18.0f,
                   label.color, 2.4f);
        }
      }
      if (state.focused) {
        FocusRing(button, 8.0f);
      }
      FillRect(ImRect(ImVec2(cx, cy + row_height), ImVec2(cx + column_width, cy + row_height + Snap(Px(1.0f)))),
               col::kDivider);
    }
    DrawLabel(Type{Font::kRegular, 13}, ImVec2(x, right.Max.y - Px(20.0f) - LineHeight(Type{Font::kRegular, 13})),
              col::kTextMuted, any ? t_.remap_note : t_.no_controllers);
  }
}

}  // namespace launcher
