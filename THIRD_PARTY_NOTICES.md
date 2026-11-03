# Third-party notices

KameoRecomp's own code is under the MIT license (see `LICENSE`). The executables built from this
repository also contain the following software, each under its own license. Full license texts are in the
`thirdparty/rexglue-sdk` submodule at the paths given below; binary releases ship them in a `licenses/`
folder.

## NativeXRenderer

The renderer, [NativeXRenderer](https://github.com/ElGuslml/NativeXRenderer) (`native_x_renderer/`, a submodule),
is MIT. It brings DiligentCore (Apache 2.0), SPIRV-Cross (Apache 2.0), volk (MIT), NVIDIA DLSS and NGX (NVIDIA RTX
SDKs License; this game offers no DLSS mode, so `nvngx_dlss.dll` is not shipped), SMAA (MIT-style), AMD FidelityFX
FSR 1 and CAS and NVIDIA Image Scaling (MIT), and the AMD FidelityFX API (MIT; `amd_fidelityfx_vk.dll` is shipped
only when the game offers AMD FSR 3.1). Their notices:
[`native_x_renderer/THIRD_PARTY_NOTICES.md`](https://github.com/ElGuslml/NativeXRenderer/blob/main/THIRD_PARTY_NOTICES.md).

NVIDIA and DLSS are trademarks or registered trademarks of NVIDIA Corporation in the U.S. and other countries.

## Kameo Launcher

The launcher (the RecompLauncher submodule, `recomp_launcher/`, and this game's `launcher/`) uses SDL 3, Dear ImGui,
toml++ and stb_image's zlib decoder from the SDK (below), and JSON for Modern C++ from the SDK's inja
(https://github.com/nlohmann/json, Copyright 2013-2023 Niels Lohmann, MIT;
`thirdparty/rexglue-sdk/thirdparty/inja/third_party/include/nlohmann/LICENSE.MIT`) to read GitHub's answer about
updates. It also contains:
- **Oswald** (`recomp_launcher/fonts/Oswald-*.ttf`, embedded in `Kameo Launcher.exe`: the titles):
  https://github.com/googlefonts/OswaldFont, Copyright 2016 The Oswald Project Authors, SIL Open Font License 1.1
  (`recomp_launcher/fonts/OFL-Oswald.txt`). Static instances (weights 500, 600, 700) of Google Fonts' variable font.
- **Source Sans 3** (`recomp_launcher/fonts/SourceSans3-*.ttf`, embedded likewise: the text):
  https://github.com/adobe-fonts/source-sans, Copyright 2010-2020 Adobe, with Reserved Font Name 'Source', SIL Open
  Font License 1.1 (`recomp_launcher/fonts/OFL-SourceSans3.txt`). Adobe's own TTF files of release 3.052R, unmodified.
- **nanosvg** (`recomp_launcher/thirdparty/nanosvg/`): https://github.com/memononen/nanosvg, Copyright (c) 2013-14
  Mikko Mononen, zlib license (`recomp_launcher/thirdparty/nanosvg/LICENSE.txt`). It draws the launcher's icons and
  art.

## ReXGlue SDK

- **ReXGlue SDK**: https://github.com/rexglue/rexglue-sdk, BSD 3-Clause, Copyright (c) 2026 Tom Clay.
  License: `thirdparty/rexglue-sdk/LICENSE`. This project builds the SDK from source: the `recomp` branch of
  https://github.com/ElGuslml/rexglue-sdk, the official nightly of 2026-10-02 (after v0.10.0) with this
  project's changes as commits.
- **Xenia**: https://xenia.jp, BSD 3-Clause, Copyright (c) 2022 Ben Vanik and Xenia project contributors.
  Portions of the ReXGlue SDK are derived from Xenia (same license file).

## Components of the SDK (`thirdparty/rexglue-sdk/thirdparty/`)

| Component | License | License file (under `thirdparty/rexglue-sdk/thirdparty/`) |
|---|---|---|
| FFmpeg (configured without GPL or nonfree parts) | LGPL 2.1 or later | `FFmpeg/COPYING.LGPLv2.1`, `FFmpeg/LICENSE.md` |
| libmspack | LGPL 2.1 | `libmspack/libmspack/COPYING.LIB` |
| SDL 3 | zlib | `sdl3/LICENSE.txt` |
| {fmt} | MIT | `fmt/LICENSE` |
| spdlog | MIT | `spdlog/LICENSE` |
| toml++ | MIT | `tomlplusplus/LICENSE` |
| Dear ImGui | MIT | `imgui/LICENSE.txt` |
| inja | MIT | `inja/LICENSE` |
| o1heap | MIT | `o1heap/LICENSE` |
| SIMDe | MIT | `simde/COPYING` |
| aes_128 | MIT | `aes_128/LICENSE` |
| Vulkan Memory Allocator | MIT | `vulkan-memory-allocator/LICENSE.txt` |
| CLI11 | BSD 3-Clause | `cli11/LICENSE` |
| Tracy | BSD 3-Clause | `tracy/LICENSE` |
| glslang | BSD 3-Clause and others | `glslang/LICENSE.txt` |
| xxHash | BSD 2-Clause | `xxHash/LICENSE` |
| utfcpp | Boost Software License 1.0 | `utfcpp/LICENSE` |
| Vulkan-Headers | Apache 2.0 / MIT | `vulkan-headers/LICENSE.md` |
| Vulkan-Loader | Apache 2.0 | `vulkan-loader/LICENSE.txt` |
| SPIRV-Tools | Apache 2.0 | `spirv-tools/LICENSE` |
| SPIRV-Headers | MIT-style (Khronos) | `spirv-headers/LICENSE` |
| stb_image | MIT / public domain | header in `stb/stb_image.h` |
| RenderDoc in-application API | MIT | header in `renderdoc/renderdoc_app.h` |

- **Barlow** and Barlow Condensed, the fonts of the SDK's dialogs (the achievement toast and list, the quit prompt;
  `thirdparty/rexglue-sdk/resources/fonts/`, embedded in the executable): https://github.com/jpt/barlow, Copyright
  2017 The Barlow Project Authors, SIL Open Font License 1.1 (`resources/fonts/OFL.txt` in the SDK).

Components used only for tests or other platforms (Catch2, MoltenVK) are not part of the Windows executables.

### LGPL components

FFmpeg and libmspack are statically linked into `rexruntime.dll`. As required by the LGPL, the complete
source code needed to rebuild that library is available: this repository and the ReXGlue SDK submodule at the
pinned commit of its public `recomp` branch (https://github.com/ElGuslml/rexglue-sdk). You can modify those
components and rebuild `rexruntime.dll` with the instructions in `README.md`.
