// 0xRRGGBB colors for Dear ImGui: what ui.h and the game's palette (palette.h) are written in.

#pragma once

#include <imgui.h>

#include <cstdint>

namespace ui {

// 0xRRGGBB as an ImGui color.
constexpr ImU32 Hex(uint32_t rgb, uint32_t alpha = 255) {
  return (alpha << IM_COL32_A_SHIFT) | (((rgb >> 16) & 0xFF) << IM_COL32_R_SHIFT) |
         (((rgb >> 8) & 0xFF) << IM_COL32_G_SHIFT) | ((rgb & 0xFF) << IM_COL32_B_SHIFT);
}

}  // namespace ui
