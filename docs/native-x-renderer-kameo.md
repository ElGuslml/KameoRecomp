# NativeXRenderer in Kameo

The game side of [NativeXRenderer](../native_x_renderer/README.md) for Kameo: Elements of Power (`default.xex`, title ID
`4D5307D2`, XDK 2.0.2099): the map of its XDK Direct3D library, the device and object layouts, the frame's structure
and the engine's behavior, with the evidence for each. The hooks and the profile are in
[`src/native_x_renderer_kameo.cpp`](../src/native_x_renderer_kameo.cpp). Facts that hold for every game are in the
library's [`xdk-d3d-reference.md`](../native_x_renderer/docs/xdk-d3d-reference.md); the decisions in its
[`decisions.md`](../native_x_renderer/docs/decisions.md) (D76, D77).

Status (2026-10-18): mapped, hooked, and verified by the observer at 100 % in the title video, the main menu, a
video and the first level. With `--native_x_renderer=true` all four draw as on the Xenos path, the first level's light
glows included ([First frames](#8-first-frames)). Off by default: Kameo draws on the Xenos path.

The XDK library was linked with **link-time code generation (LTCG)**: small D3D functions are inlined into the
engine (EndVertices, the typed Lock/Unlock thunks, CreateDevice, parts of SetVertexShader, SetPixelShader,
SetStreamSource and SetTexture), and functions with few callers are specialized for their callers' constant
arguments (Clear ignores its stencil, BeginTiling its flags, Z and stencil, EndTiling all of its arguments,
BeginIndexedVertices its primitive and stride). The callers still pass the arguments in the usual registers.

## Contents

1. [The executable](#1-the-executable)
2. [The D3D entry points](#2-the-d3d-entry-points)
3. [The device layout (XDK 2.0.2099)](#3-the-device-layout-xdk-202099)
4. [Resource objects](#4-resource-objects)
5. [Engine functions that matter for rendering](#5-engine-functions-that-matter-for-rendering)
6. [Differences from GRAW (XDK 2417)](#6-differences-from-graw-xdk-2417)
7. [Verification](#7-verification)
8. [First frames](#8-first-frames)
9. [Open](#9-open)

## 1. The executable

- `default.xex`, title ID `4D5307D2`, CRC32 `E24BE720`, XDK 2.0.2099. The library confirms the version itself: the
  device init `sub_820D92D0` passes `0x08330006` (`0x833` = 2099) to `VdInitializeEngines` (`0x820D9384`–`0x820D93A0`).
- **The D3D device code** is at `0x820BFAA8`–`0x820DAB50` (460 functions), the draws inside it
  (`0x820CE738`–`0x820CF628`), as in GRAW:
  - `0x820BFAA8`–`0x820BFED0`: bin-mask/dirty-all helper, RB_SURFACE_INFO update, window scissor writer;
  - `0x820BFED8`–`0x820C1B60`: the render-state handlers (get/set pairs, 169 small functions);
  - `0x820C1B68`–`0x820C9xxx`: setters, resolve, clears, tiling, surface/texture creation and locks, shader
    setters, constant setters, GPR allocation, XGraphics-style shader/texture helpers (`0x820C9xxx`–`0x820CCxxx`);
  - `0x820CCxxx`–`0x820CF9D8`: command buffer (allocator, reserve, kick, new segment, fences, waits), the draws,
    the vblank synchronize and Present;
  - `0x820D0048`–`0x820DAB50`: Present helper, queries, the 1421-instruction `COND_EXEC` function `sub_820D1500`
    (GRAW's `sub_824D4870`), resource Lock/Unlock/Release, the flush and its helpers, the tiling replay worker,
    device init.
  - Below it, `0x820B0000`–`0x820BFAA0`, is the engine's low-level renderer (tiling wrappers, resolve wrappers,
    surface creation wrappers) and C++ runtime helpers (`sub_820BFA60` is a vector-destructor iterator,
    `sub_820BF960` a byte swapper). Above it, `0x820DAB50`–`0x820DExxx`, is a helper library with its own state
    (objects with fields at `+772…+836`) that calls D3D: `sub_820DB8C8` / `sub_820DBEC8` (full-screen copies that
    end with a Resolve), `sub_820DD2F0` / `sub_820DD3F0` (BeginVertices wrappers), `sub_820DD180` (78 sites) and
    `sub_820DD1E0` (27 sites, shader/declaration helpers).
- **No PIX no-op stub inside the library.** The only bare `blr` with many callers is `sub_823BB178` (226 sites, all
  outside the library, `0x825xxxxx`–`0x826xxxxx`); the library uses it as the *set* handler of unused render
  states in its static table (`0x82755188`, …). Never hook it.
- **`VdSwap`'s import thunk** is `0x826CEADC`, called only from Present `sub_820CF9D8` (`0x820CFBB8`).
  `VdPersistDisplay` (`0x826CEACC`) is called from the same function (`0x820CFFC0`, when `+10433 & 0x20`).
- **The device pointer** lives in a global, `0x82766ABC` (`lis r30,-32138; lwz r3,27324(r30)` throughout the
  engine, e.g. `0x820E58E8`). It is written once by the engine's renderer init `sub_820EC830` (`0x820EC9C8`),
  which inlines Direct3D_CreateDevice: `sub_820CCD38` allocates the 20736-byte device (128-byte aligned,
  `0x820CCD7C`), the engine stores `0x04000001` at `+20560` and calls the device init `sub_820D92D0(dev,
  presentParams)`. The presentation parameters are at `0x82766AC8`, filled from the engine's settings at
  `0x8275D248` (u16 width, u16 height; runtime values, zero in the image).
- **PM4 hits outside the library** (`out/pm4.txt`) are false positives (dword counts of 3277–16384, float
  constants), except `sub_8265D4D8` and `sub_8265D6B8`, which build `SET_STATE` (0x25) and `IM_LOAD` packets into a
  buffer but are referenced by nothing but their `.pdata` entries (`0x8209F480`, `0x8209F488`): dead code, likely
  XGraphics.

## 2. The D3D entry points

Signatures are `(dev = r3, …)`; a float argument takes its integer slot (as in `calls.inc`). "Engine" counts `bl`
sites outside the library, with the number of distinct functions.

### Draws

Every draw runs the flush `sub_820D52A0` inline (5 call sites: the five draws below), then writes `DRAW_INDX`
(`0xC0032201` with an index buffer, `0xC0012201` auto-indexed). The base/start vertex goes into the shadow at
`+11540` (`VGT_INDX_OFFSET`, `0x2102`, dirty bit 61 of `+40`), not into the packet, and draws above 65535
indices are split. Inside a tiled pass with the tiling targets bound (`+10432 & 0x04`) each draw is wrapped as in
GRAW: `SET_BIN_MASK(0xFFFFFFFF, 0)`, the draw, `SET_BIN_MASK(0x80000000, 0)`, `EVENT_WRITE_EXT` (26),
`EVENT_WRITE` (25), with a `[draw − 4, 0xF00D, 0xF00D, 0xF00D]` record in the extents buffer `+13520`.

| Address | Call | Evidence | Engine |
|---|---|---|---|
| `sub_820CEF88` | `DrawIndexedVertices(dev, prim, baseVertex, startIndex, indexCount)` | `stw r5,11540` (`0x820CEFBC`); flush `0x820CEFD8`; index buffer `[+12532]` (`0x820CEFE4`): base `[ib+12]`, 32-bit indices when `[ib+0]` bit 31 (`0x820CF05C`, `0x820CF074`); `DRAW_INDX` 4 dwords `0xC0032201` (`0x820CF004`). GRAW `sub_824C4E38`, similarity 1.00. | 6 in 4: `sub_820E58C8`, `sub_820E5F48` ×3, `sub_820E7478`, `sub_820E8FF0` |
| `sub_820CED48` | `DrawVertices(dev, prim, startVertex, vertexCount)` | `stw r23(=r5),11540` (`0x820CED74`); flush; `DRAW_INDX` 2 dwords `0xC0012201` (`0x820CEDB8`). GRAW `sub_824C4BF8`, 1.00. | 10 in 9 |
| `sub_820CE738` | `BeginVertices(dev, prim, vertexCount, stride)` → vertex pointer | Zeroes `+11540`; stream 0 stride byte `+12688` set to `stride/4` around the flush (`0x820CE794`, `0x820CE7C8`); flush `0x820CE7C4`; vertex memory from the command buffer allocator `sub_820CD390(dev, dwords, 16)` (`0x820CE800`); writes fetch slot 31 as a type-0 packet `0x000548BA` (`0x820CE834`) and the `DRAW_INDX` 2 dwords **after the write pointer without moving it**; saves the end of its packets at `+13620`, the data pointer at `+13624`, the dword count at `+13632` (`0x820CE9C0`–`0x820CE9C8`). GRAW `sub_824C4578`, 0.92. | 32 in 27 (25 engine functions and the helper library's two wrappers `sub_820DD2F0`, `sub_820DD3F0`), plus DrawVerticesUP |
| `sub_820CEA50` | `BeginIndexedVertices(dev, prim, baseVertex, numVertices, indexCount, indexFormat, stride, ppIndexData = r10, ppVertexData = stack +84)` | Like BeginVertices with a second allocation for the indices; `DRAW_INDX` 4 dwords `0xC0032201` (`0x820CEC00`); saves `+13620`, `+13624`/`+13628` (vertex/index data), `+13632`/`+13636` (dword counts) (`0x820CED00`–`0x820CED18`). **LTCG-specialized:** it ignores `prim`, `stride` and `indexFormat` and hardcodes what its only caller passes: quad list (`ori r28,r30,13`, `0x820CEBB8`), 32-byte vertices (stride byte 8, `0x820CEAA0`), 16-bit indices. GRAW `sub_824C48C0`, 0.79. | 1: `sub_824FF560` (`0x824FF6B8`, passes prim 13, base 0, format 0, stride 32) |
| — | `EndVertices` | **Not a function in this executable: the engine inlines it.** `lwz rX,13620(dev); stw rX,0(dev)` at **43 sites in 38 engine functions** (list in [Open](#9-open)); DrawVerticesUP inlines it too (`0x820CEA3C`). | 43 inline sites |
| `sub_820CE9D8` | `DrawVerticesUP(dev, prim, vertexCount, data, stride)` | Kick check, BeginVertices (`0x820CEA24`), `memcpy` (`sub_82527D00`, `0x820CEA38`), inlined end `[+0] = [+13620]` (`0x820CEA3C`). GRAW `sub_824C4838`, 0.92. | 17 in 14 |
| `sub_820CF228` | `DrawIndexedTessellatedPrimitive(dev, tessPrim, baseVertex, startIndex, indexCount)` (*likely*: name from the registers it writes) | The only function that writes `VGT_OUTPUT_PATH_CNTL` (`+11700`, `0x2284`, path select 1 = tessellation, `0x820CF284`), `VGT_HOS_CNTL` is read (`+11704`), `VGT_GROUP_PRIM_TYPE`…`VGT_GROUP_VECT_1_FMT_CNTL` (`+11720`–`+11744`, `0x2289`–`0x228F`) from per-primitive tables at `0x82071730`/`0x82071810`, dirty bits in `+40`; flush; `DRAW_INDX` 4 dwords with major mode explicit (`ori r29,r8,256`, `0x820CF444`). Base vertex and start index are forced to 0 (LTCG, the caller passes 0). **No GRAW counterpart and no `calls.inc` id.** | 1: `sub_820E58C8` (`0x820E5948`), chosen per object when `[obj+20] & (1 << [0x8275D8D0+260])` |
| — | DrawIndexedVerticesUP | Not present: the five functions above are the only `DRAW_INDX` writers (`out/pm4.txt`). | — |

The library's own `DRAW_INDX_2` writers are Resolve (`sub_820C3460`), ClearF (`sub_820C4F20`), the GPR draws
(`sub_820C4698`) and a query workaround (`sub_820C4CF0`, not reached on the SDK, see QueryIssue).

### Render targets, clears, resolves, tiling, predication, viewport, scissor

| Address | Call | Evidence | Engine |
|---|---|---|---|
| `sub_820C27E0` | `SetRenderTarget(dev, index, surface)` | Slot `+12536 + 4·index` (`addi r11,r28,3134`, `0x820C27F4`); old surface: bind count `−0x80000`, fence `[+4] = [+10396]`; `[surface+52]` → RB_COLOR_INFO `+11460` (RT1–3: `+11468`, `+11472`, `+11476`), dirty bit 28 (−n) of `+32` (`0x820C2888`–`0x820C28A4`); for RT0 `sub_820BFBE8` (RB_SURFACE_INFO `+11456` ← `[surface+48]`, PA_SC_AA_CONFIG `+11772`, default scissor, SetViewport); recomputes `+10432 & 0x04`; RB_COLOR_MASK `+11548` from the per-RT masks `+11956 + 4·i`. **No alias**: the engine calls it directly. GRAW `sub_824D0C50`, 0.86. | 42 in 27 |
| `sub_820C2B78` | `SetDepthStencilSurface(dev, surface)` | `+12552` (`0x820C2BC4`); RB_DEPTH_INFO `+11464` ← `[surface+52]`, RB_HIZCONTROL `+11648` ← `[surface+56]`; bit 7 of `+10434` from `[surface+56]`; re-applies the Z states `+11972`/`+11976` (`sub_820C05C8`, `sub_820C0698`). GRAW `sub_824D0FB8`, 0.89. | 21 in 17 |
| `sub_820C4E70` | `Clear(dev, count, rects, flags, D3DCOLOR, Z = f1, stencil = r9)` | Unpacks the color (`vupkd3d128`) and calls ClearF once per rect or once with no rect. **Passes stencil 0** to ClearF (`li r8,0`, `0x820C4EC4`, `0x820C4EEC`): r9 is stored and never read (LTCG: all three callers pass 0). GRAW `sub_824CDCC8`, 0.89. | 3 in 3 |
| `sub_820C4F20` | `ClearF(dev, flags, rect, float4 color*, Z = f1, stencil = r8)` | Clips to the viewport `+12808` and, if `+11952`, the scissor `+12832`; the tiled size `+13456/+13460` when the tiling targets are bound; partial register flush `sub_820D54C0`; `IM_LOAD_IMMEDIATE` ×2 (`0x820C50FC`, `0x820C5150`), SQ_PROGRAM_CNTL written directly (`0x00012180`, `0x820C5180`), fetch slot 0 written directly (`0x00054800`), `DRAW_INDX_2` bin-predicated (`0xC0003601`); stencil into RB_STENCILREFMASK (`0x820C51F0`). GRAW `sub_824CD598`, 0.95. | 21 in 17, plus Clear, Resolve, BeginTiling |
| `sub_820C3460` | `Resolve(dev, flags, srcRect, destTexture, destPoint, level, slice, clearColor, Z = f1, stencil, params)` | Flags bits 0–2 RT0–3 / 4 = depth, sample select from `[surface+48]` (`0x820C34C0`); destination fetch constant at `destTexture+16…+36` (`0x820C3514`–`0x820C3560`); RB_BC_CONTROL `+10400` written as register `0x0F01`; RB_COPY_* (`0x2318`–`0x2323`); `EVENT_WRITE`, `IM_LOAD_IMMEDIATE`, `DRAW_INDX_2`, `WAIT_REG_MEM`; clear flags `0x100`/`0x200` → ClearF (`0x820C4054`), depth clear values `sub_820C2EF8`. GRAW `sub_824D1960`, 0.88. | 22 in 17 |
| `sub_820C41C0` | `BeginTiling(dev, flags, count, tileRects, clearColor, Z = f1, stencil)` | Count → `+12972` (`0x820C41E8`), rects → `+12976 + 16·i` and 32-aligned copies at `+13216`, max extents → `+13456/+13460`, `+13464 = 0`; saves viewport/scissor, sets the defaults (`0x820715B8`, `0x820715D0`), **ClearF(dev, 0x3F, full rect, clearColor, Z = 1.0, stencil 0)** unconditionally (`0x820C438C`, Z from `0x8207CA10`), restores; clear color → `+13472` (or zeros), Z 1.0 → `+13488`, stencil 0 → `+13492`; reference surface (RT0, else depth) → `+12968` with a bind-count reference; `+12964 = 2`, `+12960 = 0`, `+10432 |= 0x10`; tiling header start `sub_820D6018`, header word `0x86000000` into `+13540`; `EVENT_WRITE(25)`; SetViewport(default); `sub_820BFAA8(dev, 3)`. **LTCG-specialized:** flags, Z and stencil are ignored (`mr r4,r28` at `0x820C41E0`; its only caller passes 0, 1.0, 0). GRAW `sub_824D2868`, 0.78. | 1: `sub_820B1998` (`0x820B1A6C`) |
| `sub_820C44E8` | `EndTiling(dev)` — all other arguments ignored | SetPredication(0) (`0x820C44FC`), kick; with more than one tile (`[+12972] != 1`): SetPredication(3), a GPU interrupt callback through `sub_820CDE68` (callback `sub_820D5FA8`, data = the tiling header's physical address) (`0x820C458C`); kick; header word `0x86100000`; releases the reference surface `+12968`; clears `+12968`, `+12960`, `+10432 &= 0xEB`; replay submission `sub_820D6158(dev, tileCount, mask)` (`0x820C4650`); SetViewport(default). Returns `0x8007000E` if the replay fails. **No resolves**: XDK 2417's resolve arguments are not read (r4 is overwritten at `0x820C44F4`); the engine passes `r4`–`r10` = 0 except `r7` (`0x820B1EA4`–`0x820B1EC4`). GRAW `sub_824D2CA0` (no close match: the function is shorter). | 1: `sub_820B1B10` (`0x820B1EC4`) |
| `sub_820C40F0` | `SetPredication(dev, mask)` | `+12960` (`0x820C4108`); recomputes `+10432 & 0x04`; `SET_BIN_MASK(mask or 0xFFFFFFFF, 0)` (`0xC0015000`, `0x820C4180`). Unlike GRAW it does not write `+12964`. GRAW `sub_824D2730`, 0.64. | 2 in `sub_820B1B10`, 2 in EndTiling |
| `sub_820C24D0` | `SetViewport(dev, viewport*)` | Clips `{X, Y, W, H}` (u32) to the bound target (its size `[surface+24]`, or the tiled size `+13456/+13460`); stores `+12808…+12820`, MinZ/MaxZ `+12824/+12828` (`0x820C2588`–`0x820C25B0`); calls SetScissorRect(dev, dev+12832) (`0x820C25B4`); PA_CL_VPORT_* `+11592…+11612` (`0x210F`–`0x2114`) with dirty bits 48–43 of `+40`. GRAW `sub_824D0488`, 0.91. | 21 in 16 |
| `sub_820C2408` | `SetScissorRect(dev, rect*)` | `+12832…+12844` (`0x820C2448`–`0x820C2458`); if `+11952` intersects with the viewport; window scissor TL/BR copies `+11524/+11528`; writes them through `sub_820BFCB0` (type-0 `0x00022080`: PA_SC_WINDOW_OFFSET/SCISSOR_TL/BR, per tile bin-predicated `SET_CONSTANT`s while tiling). GRAW `sub_824D03C0`, 1.00. **Called by the engine** (GRAW: library only). | 7 in 5 |
| — | BeginZPass / EndZPass | Not present (absent in 2417 too). | — |

### Shaders, vertex declaration, stream source, indices, textures, constants

| Address | Call | Evidence | Engine |
|---|---|---|---|
| `sub_820C8968` | `SetVertexShader(dev, shader)` | Ref `[vs+4]++`; app's shader at `+20440` (`0x820C89AC`), then `sub_820C8810(dev, shader)` (`0x820C89B4`). GRAW `sub_824C9098`, 1.00. | 44 in 41 |
| `sub_820C8810` | The active vertex shader setter `(dev, shader)` | `+12948` (`0x820C8850`); `+10433 &= ~1`; `0x80 | 0x400` into `+48`; ORs the shader's VS constant mask into `+16`, bit 31 of `+32`; copies the shader's constant records (`vs+592+[vs+604]`) into the shadow at `dev+1152+offset` (`0x820C8878`–`0x820C8958`). GRAW `sub_824C8F40`, 1.00. **The engine calls it directly** in 9 functions that inline the outer part (`+20440`, e.g. `sub_820E5F48` `0x820E62F0`–`0x820E62F8`). | 11 in 9, plus SetVertexShader |
| `sub_820C84B8` | `SetPixelShader(dev, shader)` | `+12944` (`0x820C84F4`); `0x100 | 0x20` into `+48`; PS constant mask into `+24`; constant records `ps+52+[ps+64]` copied into the shadow. GRAW `sub_824C8D28`, 1.00. The engine also inlines the store and masks in 11 functions (e.g. `sub_820E5F48` `0x820E63B4`, `sub_820B7128` `0x820B7B18`). | 75 in 65 |
| `sub_820C89D0` | `SetVertexDeclaration(dev, declaration)` | `+11408` (`0x820C8A00`), ref `[decl+4]`, release through the game's XMemFree `sub_820B9480`; `0x80 | 0x400` into `+48`. GRAW `sub_824CA600`, 1.00. | 42 in 41 |
| `sub_820C26C0` | `SetStreamSource(dev, stream, buffer, offset, stride)` | `{offset, buffer}` at `+12556 + 8·stream` (`0x820C2748`, `0x820C2750`), `stride/4` byte at `+12688 + stream` (`0x820C2754`), `0x400` into `+48`; **no fetch constant**: the flush's stream pass `sub_820D4020` writes vertex fetch `95 − s` (`+1912 − 8·s`: `[vb+12] + offset | 3`, `[vb+16] − offset`) and its dirty bit. GRAW `sub_824D06D0`, 1.00. Inlined by the engine in 4 functions (`sub_82263020`, `sub_82263178`, `sub_823534E8`, `sub_824DCA18`). | 17 in 14 |
| `sub_820C2760` | `SetIndices(dev, buffer)` | `+12532` (`0x820C27C4`); old buffer fenced (`[+4] = [+10396]`). GRAW `sub_824D0770`, 1.00. | 6 in 4 (the DrawIndexedVertices callers) |
| `sub_820C76F0` | `SetTexture(dev, sampler, texture)` | Pointer `+12704 + 4·sampler` (`addi r11,r4,3176`, `0x820C76FC`); copies the fetch constant `texture+16…+36` into slot `sampler` at `+1152 + 24·sampler` (`mulli r11,r11,24` with `r11 = sampler+48`), keeping the sampler bits; mip/LOD bytes `+12046`, `+12072`, `+12098` + sampler; dirty bit `63 − sampler` of `+32` (`0x820C7830`). GRAW `sub_824C6F88`, 0.89. The engine unbinds samplers 0–3 inline (`stw 0,12704…12716`) in 23 functions (e.g. `sub_820B1B10` `0x820B1C7C`): pointer and fence only, no fetch constant or dirty bit. | 66 in 43 |
| `sub_820C7DC0` | `SetVertexShaderConstantF(dev, start, data, count)` | `+1920 + 16·start` (`addi r10,r4,120`, ×16), dirty groups in `+16`. GRAW `sub_824C8788`, 1.00. | 76 in 50 |
| `sub_820C7F08` | `SetPixelShaderConstantF(dev, start, data, count)` | `+6016 + 16·start` (`addi r10,r4,376`), dirty `+24`. GRAW `sub_824C88D0`, 1.00. | 37 in 29 |
| `sub_820C8050`, `sub_820C80A8`, `sub_820C8100`, `sub_820C8150` | Bool / int constant setters | Bools `+10112…` (`addi r10,r10,2528`, ×4), loops `+10144` (`2536`) and `+10208` (`2552`); bit 31 of `+32`. GRAW `sub_824C8A18`…`sub_824C8B18`, 1.00. | 3 each, all from `0x82573xxx` (another library) |
| `0x820BFED8`–`0x820C1B60` | Render-state handlers | Set table at `dev + 96 + state` (97 states, state = byte offset 0…384), get table at `dev + 564 + state`, sampler-state tables at `+484` / `+952`; filled by `sub_820D8C88` (`0x820D8CB0`–`0x820D8CF4`) from the static `{get, set, default}` table at `0x82755130` (sampler states `0x827555C0`), which calls each set handler with its default. The engine calls them through the device (`lwz r11,176(r3); mtctr; bctrl`, `sub_820B1B10` `0x820B1B34`…) and a few directly (`sub_820BFF90` RB_BLENDCONTROL ×5 and `sub_820C05C8` RB_DEPTHCONTROL ×2 from `sub_82100850`/`sub_82100270`). Named by register in `out/scratch/rs_setters.txt`. | — |
| `sub_820D4BA0` | Program flush (internal) | Called by the flush when `+48 & 0x1E0`; `INVALIDATE_STATE` (`0xC0003B00`, `0x7FFF`), `IM_LOAD` (`0xC0012700`) of the VS (`[vs+40]`, `[vs+600]` bytes, patched in place for the stream strides by `sub_820D4108` and flushed with FlushCpuRange, `0x820D4D28`–`0x820D4D38`) and the PS (`[ps+12] | 1`, `[ps+60]/4` dwords, `0x820D4E98`–`0x820D4EC0`); SQ_PROGRAM_CNTL/CONTEXT_MISC/INTERPOLATOR_CNTL go into the shadow (`+11624`, dirty `7 << 40` of `+40`). **No `LOAD_ALU_CONSTANT` anywhere in the executable**: literals are in the shadow, as in GRAW. GRAW `sub_824D98C8`, 0.89. | — |
| `sub_820C8B70` | `SetShaderGPRAllocation(dev, flags, vertexGprs, pixelGprs)` | `+10404` = packed GPRs (default `0x40400`, `0x820C8BA8`); `WAIT_UNTIL` (`0x05C8`), 9× `SQ_GPR_MANAGEMENT` (`0x0D00`), then `sub_820C4698`: `IM_LOAD_IMMEDIATE` ×2, SQ_PROGRAM_CNTL and fetch slot 0 written directly, `DRAW_INDX_2`, in the command buffer (as GRAW). GRAW `sub_824C9200`, 1.00. | 4 in 2: `sub_82255718` (0, 0, 0) before the Present helper; `sub_82271BB8` (0, 112, 16), (0, 32, 96), (0, 16, 112) |

### Resources, command buffer, queries, frame

| Address | Call | Evidence | Engine |
|---|---|---|---|
| `sub_820D3508` | `VertexBufferLock(buffer, offset, size, data, flags)` | Address `[vb+12] + offset`, size `[vb+16] & 0x03FFFFFC` when 0; the common Lock `sub_820D2E10` with kind 46 (`li r5,46`, `0x820D3550`) and the fence `[vb+4]`. GRAW `sub_824D6690`, 1.00. | 4 in 4. The engine inlines the same code in most places and calls the common Lock itself |
| `sub_820D2E10` | Common Lock `(resource, fence, kind, …)` | Waits for the resource's fence (`WAIT_REG_MEM` on the write-back block, `sub_820CDD80`). GRAW `sub_824D61F0`, 0.81. | 15 in 14 (inlined VB locks, kind 46) |
| — | VertexBufferUnlock, IndexBufferLock, IndexBufferUnlock, TextureUnlock | **Not functions in this executable**: inlined by the engine as calls to the common Unlock (e.g. the texture fills `sub_820EC4A8`…`sub_820EC740`: `CommonUnlock(tex, [tex+20] & ~0xFFF, [tex+36] & ~0xFFF)`, `0x820EC558`–`0x820EC56C`), or as a direct FlushCpuRange (VB/IB writers such as `sub_821128B0`). | — |
| `sub_820D30F8` | Common Unlock `(resource, baseAddress, mipAddress)` | Lock count in bits 12–15 of `[res+0]` (step `0x1000`, `0x820D3114`); at zero, FlushCpuRange over the dirty line ranges `[res+8]` (base) and `[res+12]` (mips), `0xFFFF0000` = none. GRAW `sub_824D6410`, 0.92. | 17 in 16 |
| `sub_820C7628` | Surface UnlockRect `(surface)` (*likely*) | `b CommonUnlock([surface+44], [surface+20] & ~0xFFF, [surface+36] & ~0xFFF)`: the parent texture is at `+44`. GRAW `sub_824C5228`, 1.00. | 4 in 3, all in `0x82543xxx` (video/texture helpers, likely Bink's) |
| `sub_820D6908` | `FlushCpuRange(start, end)` | `dcbf` over `[start, end)` in 128-byte lines, then `sync`; skips `0x7F100000`–`0x86FFFFFF` (`0x820D690C`–`0x820D6918`). GRAW `sub_824DB490`, 1.00. | 18 in 17 (+11 library): the one place every CPU write to a resource passes |
| `sub_820CDC88` | `KickSegment(dev)` → new write pointer | Outside a tiling recording (`+13504 == 0`, `+10432 & 0x80` clear) the ring submission path (`sub_820CD7E8`, `sub_820CDBC8`); then the kick tail `sub_820CDA88`: the segment fence `sub_820CD9B0` (two `EVENT_WRITE_SHD`: ring position and fence `[+10396]` → write-back block `[+10384]`, `+10396 += 2`, `0x820CDA74`), submission, then **continues in the same memory with the pointer aligned up to 32 bytes**, or NewSegment past the threshold (`0x820CDB60`–`0x820CDB84`). Inside a tiling recording it appends `[0x81000000 | dwords, address]` to the tiling header `+13540` instead (`0x820CDABC`–`0x820CDB18`). Returns `[dev+0]` (`0x820CDD64`). GRAW `sub_824D7E80`, 0.93. | 5 in 4 (inline DrawVerticesUP-style prologues, e.g. `sub_8213DB58`), 86 library |
| `sub_820CD458` | `NewSegment(dev, dwords)` | Allocates `(dwords + 12)·4` bytes through the callback `[+13604]` (context `[+13608]`, `0x820CD4A4`–`0x820CD4BC`); `+13996` = segment start, `[+0] = start − 4`, `[+4]` = end, `[+8] = end − 160`, `+13984 = +14000 = 0` (`0x820CD4D4`–`0x820CD504`). Callers: the kick tail, the reservation `sub_820CCE00(dev, dwords)` (kick, then NewSegment if it still does not fit), Present (`[+13980]/4` dwords, `0x820D000C`). GRAW `sub_824D7C08`, 0.87. | 0 (3 library) |
| `sub_820CD390` | Command buffer allocator `(dev, dwords, alignment)` | Through the same callback (kind 1); side allocations for inline draws, clears, resolves. GRAW `sub_824D71D0`, 1.00. | 0 (12 library) |
| `sub_820D07E0` | `QueryIssue(query, flags)` | `[q+0]` = device (`0x820D07FC`), `[q+4]` type (9 occlusion, 10 extents: `EVENT_WRITE(25)` / `EVENT_WRITE_EXT(26)` with `+10434 & 0x02`); BEGIN (flags bit 1) writes `0xFFFFFEED` into `+0, +8, +16, +24` of the last tile's block (`[q + 24 + 4·(tiles − 1)]`, tiles `[q+144]`); packets through `sub_820D0188`: RB_MODECONTROL, RB_COLOR_INFO, RB_SAMPLE_COUNT_ADDR (`0x2325`) = block + 32 (BEGIN) / + 0 (END), per tile with bin-predicated `SET_CONSTANT`s, `EVENT_WRITE_ZPD` (`0xC0005B00`, 21), then re-dirties those registers; END stores the fence `[+10396]` at `[q+20]`. When `VdGetGraphicsAsicID()` is not `0x11` it also runs a dummy draw (`sub_820C4AB0`, `sub_820C4CF0`); **the SDK returns `0x11`**, so that path is not taken. GRAW `sub_824D3B48`, 0.97. | 7 in 2: `sub_8213C240`, `sub_823550D8` |
| `sub_820D0968` | `QueryGetData(query, data, …)` | Pending (returns 1, writes 1) while the last block's four markers are all `0xFFFFFEED` (kicks if the END fence is current); else `Σ` per tile `ZPass_A + ZPass_B (+16/+20) − begin (+48/+52)` with byte-reversed loads; returns "done" with 1 when `+10433 & 0x04`. GRAW `sub_824D3CF0`, 0.69. | 6 in 2 (the same two) |
| `sub_820D0548`, `sub_820D0608` | CreateQuery `(dev)` → query; query release | 148-byte object: type 9, refcount `+12 = 1`, tiles `+144 = 1`, blocks from `sub_820D03E0`. | 4 / 6 |
| `sub_820CF868` | The vblank synchronize `(dev)` | Skips if `[+13756] == 0x80000000` or no front buffer yet (`[+13960]`); `WAIT_REG_MEM` on `[+10388]+4`; maps the present interval `+13756` (0/1 → 1, 2 → 2, 4 → 3), descriptor `interval << 16 | threshold` (`+11452` bits 23–29), GPU interrupt callback `sub_820CF798` through `sub_820CDE68` (`0x820CF974`), second `WAIT_REG_MEM`; `+10433 |= 0x40`. The 60 FPS hook is at `0x820CF918`. GRAW `sub_824CC858`, 1.00. | 0: called by the Present helper and the tiling worker |
| `sub_820CF9D8` | `Present(dev, frontBuffer, scalerParams)` — the swap | Front buffer reference: old `[+13960]` fenced and released, `[+13960] = frontBuffer` (`0x820CFA14`); `VdGetSystemCommandBuffer`; reserves 64 dwords (`sub_820CCE00`, `0x820CFB7C`) and **calls `VdSwap`** with `frontBuffer+16` (the fetch constant) (`0x820CFBAC`–`0x820CFBB8`), then `[+0] = reserve + 256`; scaler parameters (`r5`) compared through `sub_820D8B28`; optional `VdPersistDisplay` (`+10433 & 0x20`); kick, `sub_820DA8B8`, NewSegment(`[+13980]/4`), `sub_820DA608`, `sub_820BFAA8(dev, 0)` (all dirty). `r4` is the front buffer, as NativeXRenderer's `kPresent` expects. GRAW `sub_824CC9C8`, 0.92. | 0: called by the Present helper and the tiling worker |
| `sub_820D0048` | The XDK's Present helper `(dev)` | SetRenderTarget(dev, 0, back buffer `[+13948]`) (`0x820D00B4`), Synchronize (`0x820D00BC`), **Resolve(dev, 0, NULL, front buffer `[+13944]`, …)** (`0x820D0140`), kick, restores viewport and scissor, Present(dev, `[+13944]`, dev+13800) (`0x820D0170`). No GRAW counterpart (GRAW's engine resolves and swaps itself). | 1: the render thread loop `sub_82255718` (`0x82255A60`) |
| `sub_820C1B38` | Present interval setter (render state 380) | `stw r4,13756(r3)`; get `sub_820C1B40`; the threshold setter `sub_820C1B48` (`+11452` bits 23–29, state 384). Through the render-state table only. | 0 |
| `sub_820CDE68` | Insert GPU interrupt callback `(dev, writePtr, data, callback, argument)` → write pointer | `WAIT_REG_MEM` + `INTERRUPT` packets. GRAW `sub_824D7918`, 1.00. | 0 (3 library) |
| `sub_820D92D0` | Device init `(dev, presentParams)` | Kernel engines, ring (`sub_820CE270`, `ME_INIT`), write-back (`sub_820D6360`, which also starts the tiling worker thread `sub_820D5EC0`), default state `sub_820DA9A0`, default surfaces `sub_820D8E60`, render-state defaults `sub_820D8C88`. | 1: `sub_820EC830` |
| `sub_820CDD80`, `sub_820CD578`, `sub_820CD0C8`, `sub_820D57F8`; `sub_820D6D30` | GPU wait loops (fence, ring space, write-back, tiling worker) and the per-iteration hang check | Already hooked by `KameoGpuWait*Hook` (`kameo_functions.toml`). | — |

**The tiling worker thread** `sub_820D5EC0` (started by `sub_820D6360`, `addi r7,r10,24256` at `0x820D645C`)
builds the per-tile replays (`sub_820D5C90` → `sub_820D5680` `SET_BIN_SELECT`, `sub_820D57F8` `INDIRECT_BUFFER`)
and, when its wait times out and `+10434 & 0x08`, calls Synchronize and **Present(dev, `[+13960]`, 0)** itself
(`0x820D5F40`, `0x820D5F50`): a re-present on another thread than the render thread.

## 3. The device layout (XDK 2.0.2099)

The device is 20736 bytes (`sub_820CCD38`, `0x820CCD7C`). The flush `sub_820D52A0` is GRAW's flush with the same
masks, constants and shadow; the only change there is that the dirty fetch constants are flushed twice
(`sub_820D3E00` at `0x820D53A0` and again at `0x820D53B8`).

| Field | Offset | Evidence (function, instruction) |
|---|---|---|
| Command buffer write pointer (last dword written, `stwu`) / segment end / kick threshold | `+0` / `+4` / `+8` | NewSegment `0x820CD4F0`, `0x820CD500`, `0x820CD504`; every packet writer (`lwz r30,0(r31)`, `lwz r11,8(r31)` in BeginVertices `0x820CE7D8`) |
| Dirty mask: VS float constants, group g (4 vec4) = bit 63 − g | `+16` | Flush `0x820D5330` (`ld r11,16`; `li r5,0x4000`); `sub_820C7DC0` `0x820C7EF0` |
| Dirty mask: PS float constants | `+24` | Flush `0x820D5354` (`li r5,0x4400`) |
| Dirty mask: fetch slots (high half, slot s = bit 63 − s), bool/loop (bit 31), group `0x2000` (register `0x2000 + i` = bit 29 − i, 16 registers) | `+32` | Flush `0x820D5384` (`rldicr r4,r11,0,31`), `0x820D53C0` (`rlwinm 0,0`), `0x820D53EC`/`0x820D5400` (`rlwinm 2,17`, `rldicr 34,15`); SetTexture `0x820C7830`; SetRenderTarget bit 28 = RB_COLOR_INFO |
| Dirty mask: groups `0x2100` (bit 63 − i, 21), `0x2180` (bit 42 − i, 5), `0x2200` (bit 37 − i, 12), `0x2280` (bit 25 − i, 21) | `+40` | Flush `0x820D541C`, `0x820D5440`, `0x820D5464`–`0x820D5484`, `0x820D5494`/`0x820D54A8`; draws bit 61 = VGT_INDX_OFFSET (`0x820CEFC4`) |
| Dirty mask: groups `0x2300` (bit 63 − i, 38), `0x2380` (bit 25 − i, 8); program (`0x1E0`: VS `0x80`, PS `0x100`, `0x20`); streams (`0x400`) | `+48` | Flush `0x820D52B4`–`0x820D5328` (`rlwinm 21,21` → `sub_820D4020`; `rlwinm 23,26` → `sub_820D4BA0`; `rldicr 0,37`; `rlwinm 6,13`) |
| Render-state set / get tables (function pointers, `+ state`) | `+96` / `+564` (samplers `+484` / `+952`) | `sub_820D8C88` `0x820D8CCC`, `0x820D8CD4`, `0x820D8D34`, `0x820D8D3C` |
| Fetch constants (32 × 24 bytes, `0x4800`) | `+1152` | Flush `0x820D5390`; SetTexture |
| VS / PS float constants (256 vec4 each) | `+1920` / `+6016` | Flush `0x820D533C` / `0x820D5360` |
| Bool (8) + loop (32) constants (`0x4900`) | `+10112` (loops `+10144`) | Flush `0x820D53D0`; `sub_820C8050` (`2528·4`), `sub_820C8100` (`2536·4`) |
| Write-back block pointer (`[+0]` last fence reached, `[+4]` ring position, `[+60]` CP read pointer) | `+10384` | Segment fence `sub_820CD9B0` `0x820CD9DC`, `0x820CDA60`–`0x820CDA6C`; ring wait `sub_820CD578` `0x820CD598`–`0x820CD5A0` |
| Swap/vblank block pointer | `+10388` | Synchronize `0x820CF8E4` (`WAIT_REG_MEM` on `[+10388]+4`) |
| Fence counter (+2 per segment; stamped at `[resource+4]` on unbind) | `+10396` | `sub_820CD9B0` `0x820CD9E4`, `0x820CDA74`; SetIndices `0x820C27A4` |
| RB_BC_CONTROL copy (register `0x0F01`) | `+10400` | Device init `0x820D93C8` (from GPU register `0x7FC83C04`); Resolve `0x820C3610`–`0x820C3650` |
| Current GPR allocation | `+10404` | SetShaderGPRAllocation `0x820C8BA8` |
| EDRAM word written as register `0x0F00` | `+10408` | SetRenderTarget `0x820C2994`–`0x820C2A00` |
| Flag bytes | `+10432`: `0x10` tiling, `0x04` tiling targets bound (draws record extents), `0x80` (kick skips submission); `+10433`: `0x80` allocation failed, `0x40` swap pending, `0x20` persist display, `0x04` (GetData "done"), `0x01`; `+10434`: `0x80` HiZ from the depth surface, `0x10` (init), `0x08` worker re-presents, `0x02` extents query open | BeginTiling `0x820C43F4`–`0x820C4404`, EndTiling `0x820C45FC`, SetRenderTarget `0x820C2A84`–`0x820C2A90`, draws `0x820CF0D4`, kick `0x820CDCB0`, `sub_820CD390` `0x820CD43C`, Synchronize `0x820CF9C4`, Present `0x820CFF58`, SetDepthStencilSurface `0x820C2C14`, worker `0x820D5F34`, QueryIssue `0x820D0890` |
| Current vertex declaration | `+11408` | SetVertexDeclaration `0x820C8A00`; stream pass `0x820D4028` |
| Stream strides the bound VS was patched for (u64 ×2) | `+11424`, `+11432` | Program flush `0x820D4E10`/`0x820D4E14`; stream pass `0x820D40CC`–`0x820D40D8` |
| Bool/loop mask save (u64) | `+11440` | Flush `0x820D53E0` |
| Packed render-state word (PRESENTIMMEDIATETHRESHOLD in bits 23–29) | `+11452` | `sub_820C1B48` `0x820C1B4C`; Synchronize `0x820CF950`/`0x820CF95C` |
| Register shadow `0x2000` (16), `0x2100` (21), `0x2180` (5), `0x2200` (12), `0x2280` (21), `0x2300` (38), `0x2380` (8) | `+11456`, `+11532`, `+11616`, `+11636`, `+11684`, `+11768`, `+11920` | Flush `0x820D53F8` (`li r5,8192`), `0x820D5428` (8448), `0x820D5450` (8576), `0x820D547C` (8704), `0x820D54A0` (8832), `0x820D52F8` (8960), `0x820D5318` (9088); counts from the masks |
| PA_SC_WINDOW_OFFSET / SCISSOR_TL / BR (in the `0x2080` range, not a flushed group) | `+11520` / `+11524` / `+11528` | SetScissorRect `0x820C2490`–`0x820C24AC`; written by `sub_820BFCB0` (`0x820BFD24`–`0x820BFD54`) |
| VGT_INDX_OFFSET (in the `0x2100` group) | `+11540` | Draws `0x820CEFBC`, `0x820CED74` |
| RB_COLOR_MASK | `+11548` | SetRenderTarget `0x820C2ABC` |
| Scissor test enable | `+11952` | `sub_820C0A60`; SetScissorRect `0x820C242C`; ClearF `0x820C5078` |
| Per-RT color write masks (RT0–3) | `+11956 + 4·i` | SetRenderTarget `0x820C2A88`, `0x820C2AD8`, `0x820C2B04`, `0x820C2B30` |
| Per-sampler LOD/mip bytes | `+12046`, `+12072`, `+12098` (+ sampler) | SetTexture `0x820C77E0`, `0x820C7800`, `0x820C77BC` |
| Visibility-select bytes for the draw packet | `+12234[+12299]` | Draws `0x820CEE04`/`0x820CEE10` |
| Index buffer | `+12532` | SetIndices `0x820C27C4`; DrawIndexedVertices `0x820CEFE4` |
| Render targets RT0–RT3 | `+12536…+12548` | SetRenderTarget `0x820C27F4` (`addi r11,r28,3134`, ×4), `0x820C2838` |
| Depth-stencil surface | `+12552` | SetDepthStencilSurface `0x820C2BC4` |
| Vertex buffers `{offset, buffer}`, 8 bytes per stream | `+12556 + 8·s` (buffer at `+12560 + 8·s`) | SetStreamSource `0x820C2710`, `0x820C2748`, `0x820C2750`; stream pass `0x820D4044` |
| Stream strides (bytes / 4) | `+12688 + s` | SetStreamSource `0x820C2754`; BeginVertices `0x820CE794` |
| Textures | `+12704 + 4·sampler` | SetTexture `0x820C76FC`, `0x820C7840` |
| Viewport: X, Y, W, H (u32), MinZ, MaxZ (float) | `+12808…+12828` | SetViewport `0x820C2588`–`0x820C25B0` |
| Scissor rect (x1, y1, x2, y2) | `+12832…+12844` | SetScissorRect `0x820C2448`–`0x820C2458` |
| Pixel shader; vertex shader (active) | `+12944`; `+12948` | SetPixelShader `0x820C84F4`; `sub_820C8810` `0x820C8850`; program flush `0x820D4BB4`/`0x820D4BB8` |
| Predication mask | `+12960` | SetPredication `0x820C4108` |
| Set to 2 by BeginTiling | `+12964` | BeginTiling `0x820C43F8` |
| Reference surface saved by BeginTiling (RT0, else depth) | `+12968` | BeginTiling `0x820C43DC`–`0x820C43F0`; EndTiling `0x820C45C8`, `0x820C45F4`; every `+10432 & 0x04` recomputation |
| Tile count | `+12972` | BeginTiling `0x820C41E8`; EndTiling `0x820C4524`; `sub_820BFCB0` `0x820BFD5C` |
| Tile rects (x1, y1, x2, y2) | `+12976 + 16·i` | BeginTiling `0x820C422C`–`0x820C4244` (`r11 = dev + 12980 + 16·i`, stores at `−4…+8`) |
| 32-aligned tile rects | `+13216 + 16·i` | BeginTiling `0x820C4250`–`0x820C4280`; `sub_820BFCB0` `0x820BFD84` |
| Tiled area size (W, H); `+13464` (0, read by EndTiling) | `+13456`, `+13460`; `+13464` | BeginTiling `0x820C4290`–`0x820C42A0`; SetViewport `0x820C2534`; EndTiling `0x820C4508` |
| Tiling clear color (float4) / Z / stencil | `+13472` / `+13488` / `+13492` | BeginTiling `0x820C43B8`, `0x820C43C0`, `0x820C43CC`, `0x820C43D0` |
| Tiling recording active | `+13504` | Kick `0x820CDCA0`; kick tail `0x820CDABC`; `sub_820CD390` `0x820CD3FC` |
| Extents record buffer (descriptor; current `+13528`, limit `+13532`) | `+13520` | Draws `0x820CEC24`–`0x820CEC2C`, `0x820CEC88` |
| Tiling header buffer (descriptor; current `+13548`, limit `+13552`) | `+13540` | BeginTiling `0x820C445C`–`0x820C4490`; kick tail `0x820CDAE8`–`0x820CDB18`; EndTiling `0x820C453C` |
| Command buffer allocator callback / context | `+13604` / `+13608` | NewSegment `0x820CD4A4`/`0x820CD4AC`; device init `0x820D9394` |
| Begin*Vertices: end of the packets; vertex / index data; vertex / index dword counts | `+13620`; `+13624` / `+13628`; `+13632` / `+13636` | BeginVertices `0x820CE9C0`–`0x820CE9C8`; BeginIndexedVertices `0x820CED00`–`0x820CED18`; the engine's inline ends |
| D3DRS_PRESENTINTERVAL | `+13756` | `sub_820C1B38`; Synchronize `0x820CF87C`, `0x820CF914` |
| Default scaler parameters | `+13800` | Present helper `0x820D0164` |
| Front buffer texture; back buffer surface | `+13944`; `+13948` | Present helper `0x820D00C8`, `0x820D0084` |
| Last presented front buffer | `+13960` | Present `0x820CFA04`/`0x820CFA14`; Synchronize `0x820CF888`; worker `0x820D5F4C` |
| Ring size mask | `+13968` | Ring-space wait `sub_820CD578` `0x820CD590` |
| Segment start | `+13996` | NewSegment `0x820CD4DC`; kick tail `0x820CDAA0`, `0x820CDB94` |
| App's vertex shader (the one SetVertexShader was given) | `+20440` | SetVertexShader `0x820C89AC` |

## 4. Resource objects

Identical to GRAW (XDK 2417), each offset read in Kameo's code.

| Object | Field | Offset | Evidence |
|---|---|---|---|
| Any resource | Common: refcount low byte, bind count (step `0x80000`), lock count (bits 12–15, step `0x1000`), bit 31 = 32-bit indices for an index buffer | `+0` | SetRenderTarget `0x820C2800`/`0x820C2818`; common Unlock `0x820D3114`; DrawIndexedVertices `0x820CF074` |
| Any resource | Fence of its last use (stamped on unbind) | `+4` | SetIndices `0x820C27B0`; SetTexture `0x820C7854` |
| Any resource | Dirty line ranges for Unlock (`0xFFFF0000` = none) | `+8`, `+12` | Common Unlock `0x820D3124`, `0x820D3180` |
| Texture | Fetch constant, 6 dwords (address/format `+20`, size `+24`, swizzle/filter `+28`, mips `+32`/`+36`) | `+16` | SetTexture `0x820C7720`, `0x820C7754`, `0x820C777C`, `0x820C7784`, `0x820C77DC`, `0x820C77A0`; Resolve `0x820C3514`–`0x820C3560` |
| Surface | Fetch-like block (format `+20`, size `+24`); parent texture `+44` | `+16…+40`, `+44` | Surface creation `sub_820C7210` `0x820C74B8`–`0x820C7598`; `sub_820C7628` |
| Surface | Size: width − 1 in bits 0–12, height − 1 in bits 13–25 (a 2D fetch dword 2) | `+24` | SetViewport `0x820C2540`–`0x820C2550`; ClearF `0x820C4F68`–`0x820C4F84` |
| Surface | RB_SURFACE_INFO; RB_COLOR_INFO / RB_DEPTH_INFO; RB_HIZCONTROL | `+48`; `+52`; `+56` | `sub_820BFBE8` `0x820BFBFC`; SetRenderTarget `0x820C2860`; SetDepthStencilSurface `0x820C2BF0`, `0x820C2C08`; creation `0x820C74E8`, `0x820C7510`, `0x820C7534` |
| Vertex / index buffer | Base address; size | `+12`; `+16` | Stream pass `0x820D4060` / `0x820D4058`; VertexBufferLock `0x820D3534` / `0x820D3528`; DrawIndexedVertices `0x820CF05C` |
| Query (148 bytes) | Device; type (9 occlusion, 10 extents); refcount; state (2 begun, 1 ended); END fence; block pointers per tile; tile count | `+0`; `+4`; `+12`; `+16`; `+20`; `+24 + 4·tile`; `+144` | CreateQuery `0x820D05CC`–`0x820D05F8`; QueryIssue `0x820D07FC`, `0x820D0810`, `0x820D08A0`, `0x820D094C`; GetData `0x820D09BC` |
| Vertex shader | Microcode `[vs+40]`, size `[vs+600]` bytes, flags `[vs+588]`, fence `[vs+8]`; constant records `vs+592+[vs+604]`; program control block `vs+592+[vs+612]` | — | Program flush `0x820D4C50`, `0x820D4D2C`–`0x820D4D30`, `0x820D4CA0`, `0x820D4BC4`; `sub_820C8810` `0x820C8878` |
| Pixel shader | Microcode `[ps+12]`, size `[ps+60]` bytes, fence `[ps+8]`; constant records `ps+52+[ps+64]`; program control block `ps+52+[ps+72]` (SQ_INTERPOLATOR_CNTL at `block+24`) | — | Program flush `0x820D4E98`, `0x820D4EB8`, `0x820D4ECC`, `0x820D4BD4`, `0x820D4ED0`; SetPixelShader `0x820C8514` |

## 5. Engine functions that matter for rendering

| Address | What it does |
|---|---|
| `sub_82255718` | **The render thread's message loop** (no direct caller: a thread entry). It waits for a message (`sub_822D44C0`) and dispatches by type (`0x82255958`). The frame message: `sub_820B7128` (renders the frame, its only call site, `0x822559C8`), SetShaderGPRAllocation(0, 0, 0) (`0x82255A44`), then the XDK Present helper `sub_820D0048` (`0x82255A60`) — or, when `[r27−140]` is set, only `sub_820B1A90` (closes a tiled pass) and no present (*likely*; the flag's meaning is not known). Also creates the device (`sub_820EC830`, `0x8225582C`). |
| `sub_820EC830` | Renderer init: presentation parameters at `0x82766AC8` (from `0x8275D248`), the inlined Direct3D_CreateDevice, the device pointer `0x82766ABC`. |
| `sub_82271BB8` | A render-event dispatcher (no direct caller; compares the event against name hashes): `sub_82100850` (render-state and blend handlers called directly, BeginVertices), `sub_82100270` (ClearF, Resolve ×2, inline RB_DEPTHCONTROL/shadow writes), the BeginTiling wrapper `sub_820B1998` (`0x82272044`), the end-of-tiling wrapper `sub_820B1A90` (`0x822721D8`), SetShaderGPRAllocation (0, 112, 16) / (0, 32, 96) / (0, 16, 112). |
| `sub_820B1998` | Binds RT0 / depth from the renderer state at `0x82B70A94`, sets the viewport, then BeginTiling(dev, 0, `[cfg+16]`, `cfg+20`, clearColor, 1.0, 0), `cfg = [0x82755D24]`. Only when tiling is enabled (`[0x82B70AB0]`) and not already begun. Its helper `sub_820B1780` creates the tiling surfaces (`sub_820C7210` ×2) and clears them. |
| `sub_820B1A90` → `sub_820B1B10` | The end of the tiled pass: render states through the device table, unbinds samplers 1–3 inline, then per tile SetPredication(`3 << 2i`), a depth resolve (flags `0x14`) and a color resolve (`0x300`: clear color and depth for the next tile, 0 on the last), both with the tile's rect and the destination point at its corner (`r1+112`), into two textures (`r30+40`, `r30`); SetPredication(0), EndTiling, rebinds RT0/depth, then `sub_820B1F00` (a full-screen pass through the helper library `sub_820DB8C8`). Same shape as GRAW's `sub_821959F8`. Return addresses of its resolves: `0x820B1E54` (depth), `0x820B1E84` (color). |
| `sub_820B0110`, `sub_820B0250`, `sub_820B03C8`, `sub_820B04C8` | Resolve wrappers (*likely*: each calls `sub_820BEE08`/`sub_820BE9F0` then Resolve; `sub_820B0110` has 9 callers, among them `sub_82271BB8`). |
| `sub_820B12E0`, `sub_820B13B8`, `sub_820B9520` | Surface creation wrappers (`sub_820C7210`). |
| `sub_820E5F48` | The mesh display-list interpreter (callers `sub_820E6B98`, `sub_820E6C50`): SetVertexDeclaration, the inlined SetVertexShader (`+20440`, then `sub_820C8810`) and SetPixelShader (`+12944` and the masks), SetStreamSource, SetTexture, inline constant writes, DrawIndexedVertices ×3. `sub_820E58C8` (from it): SetIndices, then DrawIndexedVertices or the tessellated draw with the index count derived from the primitive count. `sub_820E7478`, `sub_820E8FF0`: the other DrawIndexedVertices callers. |
| DrawVertices callers | `sub_82112120`, `sub_8211AA48` ×2, `sub_82263020`, `sub_82263178`, `sub_82308DF0`, `sub_823534E8`, `sub_8235BD60`, `sub_824DCA18`, `sub_824FFD48`. |
| DrawVerticesUP callers | 14 functions, among them `sub_820BEF68`, `sub_820FCBB0`, `sub_82143FF8` ×2, `sub_8224B118`, `sub_8226B680`/`sub_8226B858`/`sub_8226BA68`, `sub_8227A9E0` (tail call), `sub_82282FC0`, `sub_822839F0`, `sub_82284210`, `sub_822D2740`, `sub_8234C250`, `sub_82446D60` ×3. |
| BeginVertices callers (end inlined) | 25 engine functions plus the helper library's `sub_820DD2F0`/`sub_820DD3F0`: `sub_82100850` ×2, `sub_82113100`, `sub_821133F8`, `sub_8213C998`, `sub_8213CC38`, `sub_8213CE80` ×2, `sub_8213D638` ×2, `sub_8213DB58` ×2, `sub_82265558`, `sub_8227A320`, `sub_8227A5A8`, `sub_822D0FC0`, `sub_822D11A0`, `sub_822D13C8` (32 callers), `sub_822D16B8` (15), `sub_822D19E0`, `sub_822D1C78`, `sub_82351F88`, `sub_823524D8`, `sub_823550D8` ×2, `sub_8244FCE0`, `sub_82450780`, `sub_82450A48`, `sub_824CFBD0`, `sub_824D27E0`. BeginIndexedVertices: `sub_824FF560` only. |
| `sub_8213C240`, `sub_823550D8` | Every query: `sub_8213C240` (from `sub_8213B618`, 4 sites) issues 4 and reads 2; `sub_823550D8` (from `sub_82354D68`) creates queries (`sub_820D0548`), draws two inline quads, issues 3, reads 4, releases (`sub_820D0608`): likely the sun / lens-flare visibility. |
| `sub_820EBEF0` → `sub_820EC4A8`, `sub_820EC588`, `sub_820EC660`, `sub_820EC740` | Texture fills with an inlined TextureUnlock (common Unlock). |
| Inline device writes (scan `out/scratch/inline_scan.py`) | Dirty masks ORed inline in **67** engine functions; **VS constants** written inline in 7 (`sub_820E3E58`, `sub_82141398`, `sub_82282AD0`, `sub_82289970`, `sub_82309BB0`, `sub_823534E8`, `sub_824D27E0`; e.g. `stfs f0,3416(r10)` at `0x820E3F6C`, then `ld/or/std 16(r10)`), **PS constants** in 30 (e.g. `sub_82141398` `0x82141694`–`0x821416AC`); the register shadow in 3 (`sub_820B9920` `0x820B9F80` +11652, `sub_820B9FA8` `0x820BA238`, `sub_82100270` `0x82100594` +11656, `0x821005C0` +11636, each with its `+40` bit); `+12944` in 11, `+20440` in 8, `+12556/+12560/+12688` in 4, sampler unbinds `+12704…+12716` in 23; **`[dev+0]`: the inline EndVertices, 43 sites in 38 functions** (found by the `lwz 13620` / `stw 0` pattern; the scan, which follows the device register less well, sees 35 of the functions). Full lists in the scan's output. |

## 6. Differences from GRAW (XDK 2417)

- **Same code, mostly.** 0.9–1.0 similarity for nearly every entry point; the flush, the five masks, the
  constants, the register shadow, the write-back block, fence, flags, current objects, viewport, scissor,
  shaders and predication are at **GRAW's offsets** (everything up to `+12964`). Resource objects are identical.
- **The tiling block is smaller.** BeginTiling saves only the reference surface (`+12968`, GRAW `+12972`, −4);
  GRAW's saved RT0–3/depth (`+12976`–`+12992`) do not exist. Tile count `+12972` and rects `+12976` (−24), aligned
  rects `+13216` (−24), tiled area `+13456/+13460` (−24), `+13464` (GRAW's flags `+13488`, −24).
- **Everything after it is 32 bytes lower:** tiling clear values `+13472/+13488/+13492` (GRAW `+13504/+13520/
  +13524`), tiling recording `+13504` (`+13536`), extents buffer `+13520` (`+13552`), tiling header `+13540`
  (`+13572`), allocator callback `+13604/+13608` (`+13636/+13640`), Begin*Vertices block `+13620`–`+13636`
  (`+13652`–`+13668`), present interval `+13756` (`+13788`), scaler parameters `+13800` (`+13832`), segment
  start `+13996` (`+14028`). The app's vertex shader is 36 lower (`+20440`, GRAW `+20476`).
- **LTCG inlining and specialization** (the library was compiled into the game with link-time code generation):
  - no EndVertices, VertexBufferUnlock, IndexBufferLock/Unlock, TextureUnlock functions; CreateDevice is in the
    engine;
  - the engine inlines parts of SetVertexShader, SetPixelShader, SetStreamSource, SetTexture(NULL), render-state
    writes, and EndVertices;
  - Clear ignores its stencil, BeginTiling its flags, Z and stencil (always clears with Z 1.0, stencil 0),
    EndTiling all of its arguments (it never resolves), BeginIndexedVertices its primitive type, stride and index
    format (quad list, 32-byte vertices, 16-bit indices for its one caller).
- **A tessellated indexed draw** (`sub_820CF228`) that GRAW's executable does not have.
- **Present.** GRAW's engine calls the vblank synchronize and the VdSwap function itself; Kameo's engine calls the
  XDK's Present helper `sub_820D0048` (SetRenderTarget(0, back buffer), Synchronize, Resolve into the front
  buffer, Swap). The tiling worker thread can also present (on a timeout).
- **Smaller differences:** the flush writes the dirty fetch constants twice (`0x820D53A0`, `0x820D53B8`) and no
  longer writes GRAW's three zeros to register `0x5000`; SetPredication does not write `+12964` (BeginTiling sets
  it to 2); SetScissorRect has engine callers (7) and SetRenderTarget has no alias; the query has a dummy-draw
  path for GPU ASIC IDs other than `0x11` (not taken on the SDK); there is no PIX no-op in the library.

## 7. Verification

The trace (`--native_x_renderer_trace=true`, F9) and the observer (`--native_x_renderer_verify=true`, F10), driven
by script through four spots: the "PRESS START" video, the main menu, the first level's opening video and the
first level (Thorn's Castle, the tutorial text on screen), 1280×720 window.

The trace: one Present per frame; everything on the render thread; **no predicated tiling** at these spots (no
BeginTiling); no BeginIndexedVertices. The title video draws its Bink frame with 174 DrawVerticesUP a frame. The
first level, per frame: about 2,970 DrawIndexedVertices, 44 DrawVerticesUP, about 870 BeginVertices from the
engine (quads: 490 from `0x824D2F8C`, 299 from `0x82100FCC`), 32 resolves, 22 render target binds, 100 QueryIssue
and 70 QueryGetData. The frame: two shadow maps (149 draws each, depth resolves), 13 small passes of 34 draws
each resolved as color (`0x300`), the scene into a 1280×720 target (about 2,150 DrawIndexedVertices and 300
inline draws) resolved with flags `0x310`, then post-processing passes and the HUD.

| Run | Spot | Draws matching | What changed |
|---|---|---|---|
| `kameo_049` | all four | 0 % (the level 59 of 3,003) | First run: the profile as mapped. Programs, SQ registers and fetch constants wrong; 3,199 of the level's 22,905 calls read incompletely. |
| `kameo_052` | all four | 100 % of the hooked draws (174, 329, 1, 3,035) | The library reads guest memory through the SDK's translation (D76): the command buffer is at 0xEF0Axxxx, which it read 4 KB off. The engine's inline draws (3, 1, 2, 823) still "written outside any hooked call". |
| `kameo_054` | all four | 100 % of all draws (177, 330, 3, 3,851) | The inline draws the engine commits itself (inlined EndVertices) tracked as EndVertices with the begin's state (D77). |

The draw packets no draw call wrote are the library's own: the clears, the resolves and SetShaderGPRAllocation's
one-point draws.

## 8. First frames

`--native_x_renderer=true` (step 4, 2026-10-18; `src/options.cpp` selects the SDK's Vulkan backend then), at the
four spots of the verification. The title video, the main menu and the level's opening video drew as on the Xenos
path from the first run (`kameo_055`), at 60 FPS. The first level needed two library changes:

| What | Result |
|---|---|
| The scene's clear | Kameo clears its scene with `Clear(1, &rect, 0x3F, ...)`, the rectangle being the target's own 1280×720; the library skipped any clear with a rectangle, so the depth kept the previous frame's values, the geometry failed the depth test and an orange, sky-like gradient covered the screen. Library D78: a rectangle that covers the target clears it whole (`kameo_057`). |
| Light glows | The glow of each light (the torches) is drawn by the count of an occlusion query around a quad that writes no color, depth always, no alpha test (`sub_823550D8`). The SDK turns that depth test off, the draw had no target and was skipped, and every count was 0: no glow, on the Xenos path too. Library D79: inside a query the test is kept; the counts are 25-49 samples per glow at the traced spot and the torches glow (`kameo_060`). The Xenos path got the same fix in shared SDK patch 0026 (its pipeline caches also drop that test, so the draw gets a stencil test that always passes and keeps instead): 17-69 samples per glow on D3D12 and Vulkan, the torches glow there too. |
| Speed | 60 FPS. The overlay at the first level: the GPU at 18 % and about 50 W with NativeXRenderer, 67 % and about 143 W on the Xenos path (D3D12, 1280×720, the same spot); NativeXRenderer's GPU time 0.64 ms a frame on average. |
| Draws | 3,849 of 3,850 drawn in the level (1 without targets); 0 textures in unsupported formats; 234 shaders, 245 pipelines. |

## 9. Open

- **More of the game**: only the first level's start, the menus and two videos have been seen with NativeXRenderer.
- **The tessellated draw** `sub_820CF228` (one engine site, `sub_820E58C8`, chosen per object): no call of its own in
  the library and no tessellation in NativeXRenderer; not seen at the traced spots.
- **The scene resolve** for the antialiasing (`scene_resolve_caller`): to choose from the trace (the scene's
  1280×720 resolve with flags `0x310`, from `0x820B039C`, is the candidate).
- **The output size**: 1280×720 in the profile. The scene and its resolves are 1280×720 (the trace's resolve
  rectangles); the front buffer itself is not printed by the trace (Present comes from the XDK's present helper,
  `0x820D0174`, every frame).
- **Present from another thread**: the tiling worker `sub_820D5EC0` can present on a timeout; not seen in the
  traces (every Present on the render thread).
- **The launcher row** and the default stay for after a full play test.
