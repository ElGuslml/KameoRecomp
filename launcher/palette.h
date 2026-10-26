// This game's palette, from Gustavo's mockup: every color the launcher draws, by name (ui.h, the pages). Each game
// has its own, like game.h; the names are the same in every game. Kameo's: a dark forest green with a periwinkle
// accent (local_docs/launcher_design: each of POTCAWE's colors matched to the one in its place in Kameo's mockup).

#pragma once

#include "ui/color.h"

namespace ui::col {

inline constexpr ImU32 kBackground = Hex(0x0B110F);
inline constexpr ImU32 kSidebar = Hex(0x101815);
inline constexpr ImU32 kSidebarBorder = Hex(0x1F2B26);
inline constexpr ImU32 kCard = Hex(0x131C19);
inline constexpr ImU32 kCardRaised = Hex(0x18231F);
inline constexpr ImU32 kCardSelected = Hex(0x18241F);
inline constexpr ImU32 kChip = Hex(0x1C2924);
inline constexpr ImU32 kField = Hex(0x1C2924);
inline constexpr ImU32 kFieldBorder = Hex(0x2A3A33);
inline constexpr ImU32 kFieldDisabled = Hex(0x15201C);
inline constexpr ImU32 kDivider = Hex(0x1C2924);
inline constexpr ImU32 kTrack = Hex(0x24332D);
inline constexpr ImU32 kSegment = Hex(0x1B2722);
inline constexpr ImU32 kOutline = Hex(0x2D3D36);
inline constexpr ImU32 kDashed = Hex(0x3A4B43);
inline constexpr ImU32 kGlyph = Hex(0x22302A);
inline constexpr ImU32 kRadioOff = Hex(0x57695F);
inline constexpr ImU32 kText = Hex(0xEAF1EC);
inline constexpr ImU32 kTextSoft = Hex(0xC2CFC7);
inline constexpr ImU32 kTextSecondary = Hex(0xB3C2B9);
inline constexpr ImU32 kTextMuted = Hex(0xA2B2A8);
inline constexpr ImU32 kLabel = Hex(0x8C9D93);
inline constexpr ImU32 kTextDim = Hex(0x7A8C82);
inline constexpr ImU32 kAccent = Hex(0x86A8FF);
inline constexpr ImU32 kAccentLight = Hex(0xB4C8FF);
inline constexpr ImU32 kOnAccent = Hex(0x0B1430);
inline constexpr ImU32 kAccentTint = Hex(0x1C2545);
inline constexpr ImU32 kDonate = Hex(0xF2894B);
inline constexpr ImU32 kOnDonate = Hex(0x1E0E04);
inline constexpr ImU32 kSuccess = Hex(0x6FD39A);
inline constexpr ImU32 kSuccessTint = Hex(0x1B3A2A);
inline constexpr ImU32 kError = Hex(0xF07B6B);
inline constexpr ImU32 kErrorTint = Hex(0x3A1F1C);
inline constexpr ImU32 kGlyphA = Hex(0x6FD39A);
inline constexpr ImU32 kGlyphB = Hex(0xF07B6B);
inline constexpr ImU32 kGlyphX = Hex(0x6AA6F2);
inline constexpr ImU32 kGlyphY = Hex(0xE8C547);

// The colors of a single place each.
inline constexpr ImU32 kHeroFill = Hex(0x1B2B1E);          // under the Play page's art
inline constexpr ImU32 kHeroButton = Hex(0x101815);        // the Play page's Exit button, over the art
inline constexpr ImU32 kHeroButtonBorder = Hex(0x2D3D36);
inline constexpr ImU32 kNoticeTint = Hex(0x17281F);        // a success notice's background (the Play page)
inline constexpr ImU32 kInstallArtFill = Hex(0x12302A);    // under the first run's art
inline constexpr ImU32 kDropZone = Hex(0x0F1714);          // the first run's ISO drop zone
inline constexpr ImU32 kValidBorder = Hex(0x2F5A42);       // the card of an ISO that was verified
inline constexpr ImU32 kErrorPanel = Hex(0x1C1415);        // the card of an ISO that was not
inline constexpr ImU32 kErrorPanelBorder = Hex(0x5A2C26);
inline constexpr ImU32 kErrorBadge = Hex(0x3A1E1B);
inline constexpr ImU32 kDoneTile = Hex(0x1B3326);          // the check of the finished installation
inline constexpr ImU32 kScrollbarActive = Hex(0x4A5E54);   // a scrollbar's thumb while dragged (not in the mockup)

// The black of the Play page's title bar and of the first run's title card, over the art (game::kHeroTitleBar,
// game::kInstallTitleCard).
inline constexpr ImU32 kTitleBar = IM_COL32(10, 10, 12, 240);
// Every corner's radius is the mockup's times this (ui.cpp): 1, the launcher's own corners.
inline constexpr float kRoundness = 1.0f;
// The first run's step markers' corners (before kRoundness): 14, circles.
inline constexpr float kStepRadius = 14.0f;

}  // namespace ui::col
