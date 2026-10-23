// The Keyboard page: playing with the keyboard and the mouse (the SDK's mnk driver), the keys of each
// Xbox 360 control (keybind_*: a primary and a secondary key each) and the mouse's camera.
//
// The keys are the SDK's names (thirdparty/rexglue-sdk/src/ui/keybinds.cpp) of physical keys: the SDK maps
// SDL scancodes, as on a US keyboard (src/ui/sdl_virtual_key.cpp). The page shows each with its label in the
// user's layout. A bind may need Shift, Ctrl or Alt held ("Shift+Up"); a bare modifier never fires there, so
// the page does not accept one alone.

#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>

#include <cstdio>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "app.h"
#include "pages.h"

namespace launcher {

using namespace ui;

namespace {

constexpr uint8_t kShift = 1;
constexpr uint8_t kCtrl = 2;
constexpr uint8_t kAlt = 4;

uint8_t ModifierBit(SDL_Scancode scancode) {
  switch (scancode) {
    case SDL_SCANCODE_LSHIFT:
    case SDL_SCANCODE_RSHIFT:
      return kShift;
    case SDL_SCANCODE_LCTRL:
    case SDL_SCANCODE_RCTRL:
      return kCtrl;
    case SDL_SCANCODE_LALT:
    case SDL_SCANCODE_RALT:
      return kAlt;
    default:
      return 0;
  }
}

// The SDK's key names by scancode: the ones keybinds.cpp parses that sdl_virtual_key.cpp produces.
const std::map<SDL_Scancode, std::string>& KeyNames() {
  static const std::map<SDL_Scancode, std::string> names = [] {
    std::map<SDL_Scancode, std::string> m;
    for (int i = 0; i < 26; ++i) m[SDL_Scancode(SDL_SCANCODE_A + i)] = std::string(1, char('A' + i));
    for (int i = 0; i < 9; ++i) m[SDL_Scancode(SDL_SCANCODE_1 + i)] = std::string(1, char('1' + i));
    m[SDL_SCANCODE_0] = "0";
    for (int i = 0; i < 12; ++i) m[SDL_Scancode(SDL_SCANCODE_F1 + i)] = "F" + std::to_string(i + 1);
    for (int i = 0; i < 12; ++i) m[SDL_Scancode(SDL_SCANCODE_F13 + i)] = "F" + std::to_string(i + 13);
    for (int i = 0; i < 9; ++i) m[SDL_Scancode(SDL_SCANCODE_KP_1 + i)] = "Numpad" + std::to_string(i + 1);
    m[SDL_SCANCODE_KP_0] = "Numpad0";
    m[SDL_SCANCODE_GRAVE] = "Backtick";
    m[SDL_SCANCODE_MINUS] = "Minus";
    m[SDL_SCANCODE_EQUALS] = "Plus";
    m[SDL_SCANCODE_COMMA] = "Comma";
    m[SDL_SCANCODE_PERIOD] = "Period";
    m[SDL_SCANCODE_SEMICOLON] = "Semicolon";
    m[SDL_SCANCODE_SLASH] = "Slash";
    m[SDL_SCANCODE_BACKSLASH] = "Backslash";
    m[SDL_SCANCODE_LEFTBRACKET] = "LBracket";
    m[SDL_SCANCODE_RIGHTBRACKET] = "RBracket";
    m[SDL_SCANCODE_APOSTROPHE] = "Quote";
    m[SDL_SCANCODE_RETURN] = "Return";
    m[SDL_SCANCODE_KP_ENTER] = "NumpadEnter";
    m[SDL_SCANCODE_SPACE] = "Space";
    m[SDL_SCANCODE_TAB] = "Tab";
    m[SDL_SCANCODE_BACKSPACE] = "Backspace";
    m[SDL_SCANCODE_DELETE] = "Delete";
    m[SDL_SCANCODE_INSERT] = "Insert";
    m[SDL_SCANCODE_HOME] = "Home";
    m[SDL_SCANCODE_END] = "End";
    m[SDL_SCANCODE_PAGEUP] = "PageUp";
    m[SDL_SCANCODE_PAGEDOWN] = "PageDown";
    m[SDL_SCANCODE_LEFT] = "Left";
    m[SDL_SCANCODE_RIGHT] = "Right";
    m[SDL_SCANCODE_UP] = "Up";
    m[SDL_SCANCODE_DOWN] = "Down";
    m[SDL_SCANCODE_KP_PLUS] = "NumpadPlus";
    m[SDL_SCANCODE_KP_MINUS] = "NumpadMinus";
    m[SDL_SCANCODE_KP_MULTIPLY] = "NumpadStar";
    m[SDL_SCANCODE_KP_DIVIDE] = "NumpadSlash";
    m[SDL_SCANCODE_PRINTSCREEN] = "PrintScreen";
    m[SDL_SCANCODE_PAUSE] = "Pause";
    m[SDL_SCANCODE_CAPSLOCK] = "CapsLock";
    m[SDL_SCANCODE_NUMLOCKCLEAR] = "NumLock";
    m[SDL_SCANCODE_SCROLLLOCK] = "ScrollLock";
    return m;
  }();
  return names;
}

// A key as the page shows it: text, and an arrow icon after it for the arrow keys.
struct KeyLabel {
  std::string text;
  Icon icon = Icon::kNone;
};

KeyLabel LabelOf(std::string_view token, const Texts& t) {
  KeyLabel label;
  // The modifiers first, as the SDK parses them.
  for (;;) {
    size_t plus = token.find('+');
    if (plus == std::string_view::npos || plus == 0) break;
    std::string_view head = token.substr(0, plus);
    const char* name = head == "Shift" ? t.key_shift
                       : head == "Ctrl" || head == "Control" ? t.key_ctrl
                       : head == "Alt" ? t.key_alt
                                       : nullptr;
    if (!name) break;
    label.text += std::string(name) + " + ";
    token.remove_prefix(plus + 1);
  }
  struct Named {
    std::string_view name;
    const char* Texts::*text;
  };
  static constexpr Named kNamed[] = {
      {"Space", &Texts::key_space},       {"Return", &Texts::key_enter},
      {"Backspace", &Texts::key_backspace}, {"Delete", &Texts::key_delete},
      {"Insert", &Texts::key_insert},     {"Home", &Texts::key_home},
      {"End", &Texts::key_end},           {"PageUp", &Texts::key_page_up},
      {"PageDown", &Texts::key_page_down}, {"CapsLock", &Texts::key_caps_lock},
      {"NumLock", &Texts::key_num_lock},  {"ScrollLock", &Texts::key_scroll_lock},
      {"PrintScreen", &Texts::key_print_screen}, {"Pause", &Texts::key_pause},
      {"LMB", &Texts::mouse_left},        {"RMB", &Texts::mouse_right},
      {"MMB", &Texts::mouse_middle},
  };
  for (const Named& named : kNamed) {
    if (token == named.name) {
      label.text += t.*named.text;
      return label;
    }
  }
  if (token == "Up" || token == "Down" || token == "Left" || token == "Right") {
    label.icon = token == "Up" ? Icon::kArrowUp : token == "Down" ? Icon::kArrowDown
                 : token == "Left" ? Icon::kArrowLeft : Icon::kArrowRight;
    return label;
  }
  if (token == "Tab" || token == "Escape") {
    label.text += token == "Tab" ? "Tab" : "Esc";
    return label;
  }
  if (token.starts_with("Numpad")) {
    std::string_view rest = token.substr(6);
    std::string key = rest == "Enter" ? t.key_enter : rest == "Plus" ? "+" : rest == "Minus" ? "-"
                      : rest == "Star" ? "*" : rest == "Slash" ? "/" : std::string(rest);
    label.text += std::string(t.key_numpad) + " " + key;
    return label;
  }
  if (token.size() >= 2 && token[0] == 'F' && token[1] >= '0' && token[1] <= '9') {
    label.text += token;
    return label;
  }
  // Letters, digits and punctuation: the character on that key in the user's layout.
  for (const auto& [scancode, name] : KeyNames()) {
    if (name == token) {
      SDL_Keycode key = SDL_GetKeyFromScancode(scancode, SDL_KMOD_NONE, false);
      const char* text = SDL_GetKeyName(key);
      label.text += text && *text ? text : name;
      return label;
    }
  }
  label.text += token;
  return label;
}

std::vector<std::string> Alternatives(const std::string& list) {
  std::vector<std::string> out;
  size_t start = 0;
  while (start <= list.size()) {
    size_t comma = list.find(',', start);
    std::string item = list.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
    size_t first = item.find_first_not_of(' ');
    size_t last = item.find_last_not_of(' ');
    out.push_back(first == std::string::npos ? std::string() : item.substr(first, last - first + 1));
    if (comma == std::string::npos) break;
    start = comma + 1;
  }
  return out;
}

std::string Join(const std::vector<std::string>& items) {
  std::string out;
  for (const std::string& item : items) {
    if (item.empty()) continue;
    if (!out.empty()) out += ",";
    out += item;
  }
  return out;
}

// The pill left of each control: the Xbox 360 control it is.
struct Glyph {
  const char* text;
  Icon icon;
  ImU32 color;
};

Glyph GlyphOf(Bind bind) {
  switch (bind) {
    case Bind::kLStickUp:
    case Bind::kLStickDown:
    case Bind::kLStickLeft:
    case Bind::kLStickRight:
    case Bind::kLStickPress:
      return {"LS", Icon::kNone, col::kTextSoft};
    case Bind::kRStickUp:
    case Bind::kRStickDown:
    case Bind::kRStickLeft:
    case Bind::kRStickRight:
    case Bind::kRStickPress:
      return {"RS", Icon::kNone, col::kTextSoft};
    case Bind::kA:
      return {"A", Icon::kNone, col::kGlyphA};
    case Bind::kB:
      return {"B", Icon::kNone, col::kGlyphB};
    case Bind::kX:
      return {"X", Icon::kNone, col::kGlyphX};
    case Bind::kY:
      return {"Y", Icon::kNone, col::kGlyphY};
    case Bind::kLeftShoulder:
      return {"LB", Icon::kNone, col::kTextSoft};
    case Bind::kRightShoulder:
      return {"RB", Icon::kNone, col::kTextSoft};
    case Bind::kLeftTrigger:
      return {"LT", Icon::kNone, col::kTextSoft};
    case Bind::kRightTrigger:
      return {"RT", Icon::kNone, col::kTextSoft};
    case Bind::kDpadUp:
      return {nullptr, Icon::kArrowUp, col::kTextSoft};
    case Bind::kDpadDown:
      return {nullptr, Icon::kArrowDown, col::kTextSoft};
    case Bind::kDpadLeft:
      return {nullptr, Icon::kArrowLeft, col::kTextSoft};
    case Bind::kDpadRight:
      return {nullptr, Icon::kArrowRight, col::kTextSoft};
    case Bind::kStart:
      return {"START", Icon::kNone, col::kTextSoft};
    case Bind::kBack:
      return {"BACK", Icon::kNone, col::kTextSoft};
    case Bind::kCount:
      break;
  }
  return {"", Icon::kNone, col::kTextSoft};
}

void DrawGlyph(ImVec2 pos, float row_height, const Glyph& glyph) {
  constexpr Type kGlyphType{Font::kBold, 13};
  float width = glyph.text ? std::max(Px(40.0f), MeasureText(kGlyphType, glyph.text).x + Px(16.0f)) : Px(40.0f);
  ImRect pill(ImVec2(pos.x, pos.y + (row_height - Px(28.0f)) * 0.5f),
              ImVec2(pos.x + width, pos.y + (row_height + Px(28.0f)) * 0.5f));
  FillRect(pill, col::kGlyph, 14.0f);
  if (glyph.text) {
    DrawLabel(kGlyphType, ImVec2(pill.Min.x + (width - MeasureText(kGlyphType, glyph.text).x) * 0.5f, pill.Min.y),
             glyph.color, glyph.text, pill.GetHeight());
  } else {
    DrawIcon(glyph.icon, ImVec2(pill.Min.x + (width - Px(14.0f)) * 0.5f, pill.Min.y + Px(7.0f)), 14.0f, glyph.color,
             2.4f);
  }
}

constexpr Type kSlotType{Font::kSemiBold, 14};

}  // namespace

void App::StartListening(int bind, int slot) {
  listen_bind_ = bind;
  listen_slot_ = slot;
  held_modifiers_ = 0;
  modifier_pressed_ = false;
}

void App::StopListening() {
  listen_bind_ = -1;
  pad_listen_ = -1;
  held_modifiers_ = 0;
  modifier_pressed_ = false;
}

void App::Assign(std::string token) {
  std::string& list = settings_.o.binds[size_t(listen_bind_)];
  std::vector<std::string> alternatives = Alternatives(list);
  while (int(alternatives.size()) <= listen_slot_) {
    alternatives.emplace_back();
  }
  alternatives[size_t(listen_slot_)] = std::move(token);
  list = Join(alternatives);
  StopListening();
  Changed();
}

bool App::OnKeyCapture(const SDL_Event& event) {
  if (listen_bind_ < 0) {
    return false;
  }
  auto prefix = [this] {
    std::string text;
    if (held_modifiers_ & kShift) text += "Shift+";
    if (held_modifiers_ & kCtrl) text += "Ctrl+";
    if (held_modifiers_ & kAlt) text += "Alt+";
    return text;
  };
  switch (event.type) {
    case SDL_EVENT_KEY_DOWN: {
      if (event.key.repeat) {
        return true;
      }
      if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
        StopListening();
        return true;
      }
      if (uint8_t bit = ModifierBit(event.key.scancode)) {
        held_modifiers_ |= bit;
        modifier_pressed_ = true;
        return true;
      }
      auto it = KeyNames().find(event.key.scancode);
      if (it != KeyNames().end()) {
        Assign(prefix() + it->second);
      }
      return true;  // a key the game cannot use: still waiting
    }
    case SDL_EVENT_KEY_UP:
      if (uint8_t bit = ModifierBit(event.key.scancode)) {
        held_modifiers_ &= uint8_t(~bit);
        if (!held_modifiers_ && modifier_pressed_) {
          // Shift, Ctrl or Alt pressed and released alone.
          modifier_pressed_ = false;
          notice_ = t_.modifier_alone;
          notice_until_ = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        }
      }
      return true;
    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
      const char* name = event.button.button == SDL_BUTTON_LEFT     ? "LMB"
                         : event.button.button == SDL_BUTTON_RIGHT  ? "RMB"
                         : event.button.button == SDL_BUTTON_MIDDLE ? "MMB"
                                                                    : nullptr;
      if (name) {
        Assign(prefix() + name);
      }
      return true;
    }
    case SDL_EVENT_MOUSE_BUTTON_UP:
    case SDL_EVENT_TEXT_INPUT:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
      return true;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
      if (event.gbutton.button == SDL_GAMEPAD_BUTTON_EAST) {
        StopListening();
      }
      return true;
    default:
      return false;
  }
}

void App::KeyboardPage(const ImRect& rect) {
  Options& o = settings_.o;
  ImRect content = PageContent(rect);
  bool notice = std::chrono::steady_clock::now() < notice_until_;
  float y = Header(content.Min, t_.controls, t_.nav_keyboard, notice ? nullptr : t_.keyboard_subtitle,
                   t_.experimental_badge);
  if (notice) {
    constexpr Type kNotice{Font::kSemiBold, 14};
    DrawLabel(kNotice, ImVec2(content.Min.x, y + Px(8.0f)), col::kAccentLight, notice_);
    y += Px(8.0f) + LineHeight(kNotice);
  }
  bool changed = false;
  {
    ButtonSpec reset;
    reset.icon = Icon::kReset;
    reset.icon_size = 16;
    float width = ButtonWidth(reset, t_.reset_defaults);
    if (Button("reset", ImVec2(content.Max.x - width, y - Px(44.0f)), reset, t_.reset_defaults)) {
      StopListening();
      ResetKeyboard(o);
      changed = true;
    }
  }
  y += Px(20.0f);

  // Playing with the keyboard and the mouse at all.
  {
    constexpr Type kTitle{Font::kSemiBold, 15};
    constexpr Type kHint{Font::kRegular, 13};
    float height = Px(20.0f) * 2.0f + LineHeight(kTitle) + Px(4.0f) + LineHeight(kHint);
    ImRect card(ImVec2(content.Min.x, y), ImVec2(content.Max.x, y + height));
    FillRect(card, col::kCard, 12.0f);
    DrawLabel(kTitle, ImVec2(card.Min.x + Px(28.0f), card.Min.y + Px(20.0f)), col::kText, t_.mnk);
    DrawLabel(kHint, ImVec2(card.Min.x + Px(28.0f), card.Min.y + Px(20.0f) + LineHeight(kTitle) + Px(4.0f)),
             col::kTextMuted, t_.mnk_hint);
    if (Toggle("mnk", ImVec2(card.Max.x - Px(28.0f) - Px(52.0f), card.Min.y + (height - Px(30.0f)) * 0.5f), &o.mnk)) {
      changed = true;
    }
    y += height + Px(20.0f);
  }

  // The table: each control's primary and secondary key, then the mouse.
  ImRect table(ImVec2(content.Min.x, y), ImVec2(content.Max.x, content.Max.y));
  FillRect(table, col::kCard, 12.0f);
  constexpr Type kColumn{Font::kBold, 12, 0.1f};
  float slot_width = Px(200.0f);
  float gap = Px(16.0f);
  float header_height = Px(14.0f) * 2.0f + LineHeight(kColumn);
  {
    float x = table.Min.x + Px(24.0f);
    float right = table.Max.x - Px(24.0f);
    float top = table.Min.y + Px(14.0f);
    DrawLabel(kColumn, ImVec2(x, top), col::kLabel, t_.column_control);
    DrawLabel(kColumn, ImVec2(right - slot_width * 2.0f - gap, top), col::kLabel, t_.column_primary);
    DrawLabel(kColumn, ImVec2(right - slot_width, top), col::kLabel, t_.column_secondary);
    FillRect(ImRect(ImVec2(table.Min.x, table.Min.y + header_height),
                    ImVec2(table.Max.x, table.Min.y + header_height + Snap(Px(1.0f)))),
             Hex(0x232A35));
  }
  ImRect list(ImVec2(table.Min.x, table.Min.y + header_height + Snap(Px(1.0f))),
              ImVec2(table.Max.x, table.Max.y - Px(6.0f)));
  BeginScroll("keys", list);
  {
    ImVec2 origin = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;
    float x = origin.x + Px(24.0f);
    float right = origin.x + width - Px(24.0f);
    float slots_x = right - slot_width * 2.0f - gap;
    float y_list = origin.y + Px(4.0f);
    constexpr Type kGroup{Font::kBold, 13};
    constexpr Type kLabel{Font::kRegular, 15};
    float row_height = Px(48.0f);
    struct Group {
      const char* title;
      Bind first, last;
    };
    const Group groups[] = {{t_.group_movement, Bind::kLStickUp, Bind::kLStickPress},
                            {t_.group_camera, Bind::kRStickUp, Bind::kRStickPress},
                            {t_.group_buttons, Bind::kA, Bind::kRightTrigger},
                            {t_.group_dpad, Bind::kDpadUp, Bind::kBack}};
    for (const Group& group : groups) {
      DrawLabel(kGroup, ImVec2(x, y_list + Px(16.0f)), col::kAccent, group.title);
      y_list += Px(16.0f) + LineHeight(kGroup) + Px(6.0f);
      for (int b = int(group.first); b <= int(group.last); ++b) {
        DrawGlyph(ImVec2(x, y_list), row_height, GlyphOf(Bind(b)));
        DrawLabel(kLabel, ImVec2(x + Px(52.0f), y_list), col::kText,
                 Ellipsize(kLabel, t_.binds[b], slots_x - gap - x - Px(52.0f)), row_height);
        std::vector<std::string> alternatives = Alternatives(o.binds[size_t(b)]);
        for (int slot = 0; slot < 2; ++slot) {
          std::string token = slot < int(alternatives.size()) ? alternatives[size_t(slot)] : std::string();
          ImRect r(ImVec2(slots_x + float(slot) * (slot_width + gap), y_list + Px(5.0f)),
                   ImVec2(slots_x + float(slot) * (slot_width + gap) + slot_width, y_list + Px(43.0f)));
          ImGui::PushID(b * 2 + slot);
          ItemState state = Item("slot", r);
          ImGui::PopID();
          bool listening = listen_bind_ == b && listen_slot_ == slot;
          if (state.pressed) {
            if (listening) {
              StopListening();
            } else {
              StartListening(b, slot);
              listening = true;
            }
          }
          // Delete, Backspace, X on a controller or a right click clear a slot.
          if (!listening && !token.empty() &&
              ((state.focused && (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace) ||
                                  ImGui::IsKeyPressed(ImGuiKey_GamepadFaceLeft))) ||
               (state.hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)))) {
            alternatives.resize(std::max<size_t>(alternatives.size(), 2));
            alternatives[size_t(slot)].clear();
            o.binds[size_t(b)] = Join(alternatives);
            changed = true;
            token.clear();
          }
          if (listening) {
            FillRect(r, col::kAccentTint, 8.0f);
            StrokeRect(r, col::kAccent, 8.0f, 2.0f);
            // With Shift, Ctrl or Alt held: "Shift + …", waiting for the key.
            std::string text;
            if (held_modifiers_ & kShift) text += std::string(t_.key_shift) + " + ";
            if (held_modifiers_ & kCtrl) text += std::string(t_.key_ctrl) + " + ";
            if (held_modifiers_ & kAlt) text += std::string(t_.key_alt) + " + ";
            DrawLabel(kSlotType, ImVec2(r.Min.x + Px(14.0f), r.Min.y), col::kAccentLight,
                     held_modifiers_ ? text + "\xE2\x80\xA6" : std::string(t_.press_key), r.GetHeight());
          } else if (token.empty()) {
            DashedRect(r, state.hovered ? Brighten(col::kDashed) : col::kDashed, 8.0f);
            DrawLabel(kSlotType, ImVec2(r.Min.x + Px(14.0f), r.Min.y), col::kLabel, t_.unbound, r.GetHeight());
          } else {
            FillRect(r, state.hovered ? Brighten(col::kField) : col::kField, 8.0f);
            StrokeRect(r, col::kFieldBorder, 8.0f);
            KeyLabel label = LabelOf(token, t_);
            float text_width = MeasureText(kSlotType, label.text).x;
            DrawLabel(kSlotType, ImVec2(r.Min.x + Px(14.0f), r.Min.y), col::kText,
                     Ellipsize(kSlotType, label.text, slot_width - Px(28.0f) - (label.icon != Icon::kNone ? Px(18.0f) : 0)),
                     r.GetHeight());
            if (label.icon != Icon::kNone) {
              DrawIcon(label.icon, ImVec2(r.Min.x + Px(14.0f) + text_width, r.Min.y + (r.GetHeight() - Px(16.0f)) * 0.5f),
                       16.0f, col::kText, 2.2f);
            }
          }
          if (state.focused) {
            FocusRing(r, 8.0f);
          }
        }
        y_list += row_height;
      }
    }

    // The mouse.
    DrawLabel(kGroup, ImVec2(x, y_list + Px(16.0f)), col::kAccent, t_.group_mouse);
    y_list += Px(16.0f) + LineHeight(kGroup) + Px(6.0f);
    Glyph mouse{nullptr, Icon::kMouse, col::kTextSoft};
    DrawGlyph(ImVec2(x, y_list), row_height, mouse);
    DrawLabel(kLabel, ImVec2(x + Px(52.0f), y_list), col::kText, t_.mouse_camera, row_height);
    if (Toggle("mouse", ImVec2(slots_x, y_list + (row_height - Px(30.0f)) * 0.5f), &o.mnk_mouse)) {
      changed = true;
    }
    DrawLabel(Type{Font::kRegular, 13}, ImVec2(slots_x + Px(52.0f) + Px(14.0f), y_list), col::kTextMuted,
             Ellipsize(Type{Font::kRegular, 13}, t_.mouse_camera_hint, slot_width * 2.0f + gap - Px(66.0f)),
             row_height);
    y_list += row_height;
    DrawGlyph(ImVec2(x, y_list), row_height, mouse);
    DrawLabel(kLabel, ImVec2(x + Px(52.0f), y_list), col::kText, t_.mouse_sensitivity, row_height);
    float sensitivity = o.mnk_sensitivity;
    if (Slider("sensitivity", ImRect(ImVec2(slots_x, y_list + Px(5.0f)), ImVec2(slots_x + slot_width, y_list + Px(43.0f))),
               &sensitivity, 0.1f, 5.0f, 0.1f, o.mnk_mouse)) {
      o.mnk_sensitivity = sensitivity;
      changed = true;
    }
    char value[32];
    std::snprintf(value, sizeof(value), "%.1f×", double(o.mnk_sensitivity));
    DrawLabel(kSlotType, ImVec2(slots_x + slot_width + gap, y_list), o.mnk_mouse ? col::kText : col::kTextDim, value,
             row_height);
    y_list += row_height + Px(12.0f);
    ImGui::Dummy(ImVec2(width, y_list - origin.y));
  }
  EndScroll();

  if (changed) {
    Changed();
  }
}

}  // namespace launcher
