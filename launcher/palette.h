// This game's palette, from Gustavo's mockup: every color the launcher draws, by name (ui.h, the pages). Each game
// has its own, like game.h; the names are the same in every game. Kameo's: a dark forest green with a periwinkle
// accent (local_docs/launcher_design: each of POTCAWE's colors matched to the one in its place in Kameo's mockup); the
// shell of launcher 3.0 the template's, in the same greens.

#pragma once

#include "ui/color.h"

namespace ui::col {

inline constexpr ImU32 kBackground = Hex(0x0B110F);
inline constexpr ImU32 kCard = Hex(0x131C19);
inline constexpr ImU32 kCardRaised = Hex(0x18231F);
inline constexpr ImU32 kChip = Hex(0x1C2924);
inline constexpr ImU32 kField = Hex(0x1C2924);
inline constexpr ImU32 kFieldBorder = Hex(0x2A3A33);
inline constexpr ImU32 kTrack = Hex(0x24332D);
inline constexpr ImU32 kOutline = Hex(0x2D3D36);
inline constexpr ImU32 kDashed = Hex(0x3A4B43);
inline constexpr ImU32 kGlyph = Hex(0x22302A);
inline constexpr ImU32 kText = Hex(0xEAF1EC);
inline constexpr ImU32 kTextSoft = Hex(0xC2CFC7);
inline constexpr ImU32 kTextMuted = Hex(0xA2B2A8);
inline constexpr ImU32 kLabel = Hex(0x8C9D93);
inline constexpr ImU32 kTextDim = Hex(0x7A8C82);
inline constexpr ImU32 kAccent = Hex(0x86A8FF);
inline constexpr ImU32 kAccentLight = Hex(0xB4C8FF);
inline constexpr ImU32 kOnAccent = Hex(0x0B1430);
inline constexpr ImU32 kGlyphOnAccent = kOnAccent;  // a button's disc inside an accent button (RecompLauncher 3.2)
inline constexpr ImU32 kAccentTint = Hex(0x1C2545);
inline constexpr ImU32 kDonate = Hex(0xF2894B);
inline constexpr ImU32 kOnDonate = Hex(0x1E0E04);
inline constexpr ImU32 kDonateIcon = kOnDonate;      // Donate's heart (RecompLauncher 3.2)
inline constexpr ImU32 kSuccess = Hex(0x6FD39A);
inline constexpr ImU32 kSuccessTint = Hex(0x1B3A2A);
inline constexpr ImU32 kError = Hex(0xF07B6B);
inline constexpr ImU32 kErrorTint = Hex(0x3A1F1C);
inline constexpr ImU32 kGlyphA = Hex(0x6FD39A);
inline constexpr ImU32 kGlyphB = Hex(0xF07B6B);
inline constexpr ImU32 kGlyphX = Hex(0x6AA6F2);
inline constexpr ImU32 kGlyphY = Hex(0xE8C547);

// The colors of a single place each.
inline constexpr ImU32 kNoticeTint = Hex(0x17281F);        // a success notice's background (the Play page)
inline constexpr ImU32 kScrollbarActive = Hex(0x4A5E54);   // a scrollbar's thumb while dragged (not in the mockup)


// The shell of launcher 3.0 (Gustavo's 2026-10-28 mockup): the header and the footer over the art, the panels and their
// chips over it, the controller's keys in the hints. The template's, its blacks in Kameo's greens.
inline constexpr ImU32 kHeader = Hex(0x080D0B, 224);         // rgba(8, 13, 11, 0.88)
inline constexpr ImU32 kFooter = Hex(0x080D0B, 240);         // rgba(8, 13, 11, 0.94)
inline constexpr ImU32 kHairline = Hex(0xFFFFFF, 20);        // their borders, the panels' (0.08)
inline constexpr ImU32 kPanel = Hex(0x101815, 237);          // a card over the art (0.93)
inline constexpr ImU32 kPanelChip = Hex(0xFFFFFF, 26);       // a chip in it (0.10)
inline constexpr ImU32 kPanelTrack = Hex(0xFFFFFF, 31);      // a bar's track in it (0.12)
inline constexpr ImU32 kBumper = Hex(0xFFFFFF, 31);          // the header's LB and RB (0.12)
inline constexpr ImU32 kOnBumper = Hex(0xC2CFC7);
inline constexpr ImU32 kTabText = Hex(0xA2B2A8);
inline constexpr ImU32 kTabActive = Hex(0xFFFFFF);
inline constexpr ImU32 kFocus = Hex(0xFFFFFF);               // the focus outline, 3 dp, 4 dp out
inline constexpr ImU32 kDim = Hex(0x060A08);                 // over the art under a page (0.55 or 0.9)
inline constexpr ImU32 kHintBumper = Hex(0xFFFFFF, 36);      // the hints' LB RB and arrows (0.14)
inline constexpr ImU32 kOnHintBumper = Hex(0xE6EEE9);
inline constexpr ImU32 kOnGlyphA = Hex(0x06210F);
inline constexpr ImU32 kOnGlyphB = Hex(0x2A0B06);
inline constexpr ImU32 kOnGlyphX = Hex(0x061A33);
inline constexpr ImU32 kOnGlyphY = Hex(0x2A2205);
inline constexpr ImU32 kGhostFill = Hex(0x080D0B, 179);      // a dark button over the art (0.7)
inline constexpr ImU32 kGhostBorder = Hex(0xFFFFFF, 46);     // its border (0.18)

// Every corner's radius is the mockup's times this (ui.cpp): 1, the launcher's own corners.
inline constexpr float kRoundness = 1.0f;

}  // namespace ui::col
