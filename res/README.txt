KameoRecomp {VERSION} - Kameo: Elements of Power (Xbox 360), for Windows
========================================================================

A static recompilation of the Xbox 360 game to native Windows. The game's code is already translated and
compiled into KameoEOPRecomp.exe: you do not need to compile anything. You do need your own copy of the
game, because this package contains no game files.

This is an early test build. Not affiliated with Microsoft or Rare.


REQUIREMENTS

- Windows 10 or 11, 64-bit.
- A GPU with up-to-date drivers that support Vulkan (NativeXRenderer, the default renderer) or
  Direct3D 12 (the Xenos renderer).
- A CPU with SSE4.2 (practically any 64-bit CPU from 2009 on).
- Microsoft Visual C++ Redistributable 2015-2022 (x64). Most PCs already have it. If Windows says that
  MSVCP140.dll or VCRUNTIME140.dll is missing, install it from:
  https://aka.ms/vs/17/release/vc_redist.x64.exe
- Your Kameo: Elements of Power disc as an ISO file (the USA version; about 5.4 GB once installed).


INSTALLATION

1. Either run the installer (KameoRecomp-v{VERSION}-setup.exe: it installs for your Windows user, without
   administrator rights), or extract this zip to any folder, for example C:\Games\KameoRecomp.

2. Run "Kameo Launcher.exe". The first time, it asks for your Xbox 360 ISO, checks that it is the right
   game and extracts its files next to the launcher. You only do this once.

   You can also copy the game files yourself instead: put the contents of the disc into the "assets"
   folder, so that assets\default.xex exists (extract-xiso, https://github.com/XboxDev/extract-xiso/releases,
   gets them out of an ISO).

3. Choose your options and press PLAY. Next time you can start KameoEOPRecomp.exe directly: it uses the
   options you saved.

If your default.xex is not the version this build was made from (CRC32 E24BE720), the game warns you: a
different version will most likely crash.


"WINDOWS PROTECTED YOUR PC" / UNKNOWN PUBLISHER

The executables are not code-signed, so Windows SmartScreen warns about an unknown publisher the first time
you run them. To avoid it with the zip, unblock it before you extract it: right-click it, Properties, tick
"Unblock" at the bottom of the General tab, OK, and then extract it. Otherwise click "More info" and then
"Run anyway" in the warning (once for each program).


KAMEO LAUNCHER

The launcher works with the mouse, the keyboard or a controller. Changes are saved automatically and apply
when you press PLAY.

- Play: starts the game, with your current settings, achievements and updates at a glance.
- Achievements: the game's 26 achievements, unlocked or not. In the game, F7 shows the same list.
- Controls:
  - Keyboard (experimental): play with the keyboard and mouse as player 1's controller, and change any key.
  - Gamepad: change what each button of your controller does (Xbox, PlayStation and other controllers).
- Settings:
  - Video: the monitor, fullscreen, borderless or window, the resolution, the image format (fill a screen
    wider than 16:9, where the game sees more to the sides while the HUD, menus and videos stay 16:9 in the
    center, or keep 16:9 with bars) and the internal resolution the game is drawn at.
  - Graphics: the renderer, the frame rate, antialiasing, anisotropic filtering and shadow quality.
    NativeXRenderer (the default) draws the game natively with Vulkan; Xenos emulates the Xbox 360 GPU
    instead (DirectX 12 or Vulkan): choose it if something looks wrong with NativeXRenderer, and tell us
    where. Frame rate: 30 FPS (original), 60 FPS, and 120 to 240 FPS where your monitor reaches them (marked
    experimental). The game speed stays the same at every frame rate.
  - Game: the language (English, French, German, Italian, Spanish from Spain or Latin America, Japanese,
    Korean or Traditional Chinese; the game has no language menu of its own, and Korean and Traditional
    Chinese have text only: the disc has no voices for them) and the in-game cinematics without black bars.
- Updates: the launcher checks for a new version when it starts (you can turn that off) and installs it
  for you, keeping your saves, settings and game files.


QUITTING THE GAME

On the title screen (PRESS START), press B: the game asks whether to quit. A (or Enter) quits, B (or Escape)
goes back to the title screen.


STEAM BIG PICTURE

Add KameoEOPRecomp.exe as a non-Steam game: it starts straight into the game with the options saved by
Kameo Launcher. You can also add "Kameo Launcher.exe" to change options with a controller.


FILES

- kameo.toml           Base settings. Created with the defaults this game needs if it is missing.
- kameo_options.toml   What Kameo Launcher saves. Loaded after kameo.toml.
- logs\                Log files. Attach the latest one when you report a problem.
- userdata\            Saves, profile, achievements and shader cache. Keep this folder if you move the game
                       to another folder. If the game folder cannot be written (for example under Program
                       Files), they go to Documents\kameo instead.

Command line options (--name=value, for example in Steam launch options) win over both files, e.g.
--user_language=1 (English), --kameo_frame_rate=60 or --native_x_renderer=false (the Xenos renderer).


KNOWN ISSUES

- Played so far: Thorn's Castle (the first level) and the first areas and missions of the Enchanted
  Kingdom. Anything after that is untested: reports are welcome, with a screenshot and the latest log.
- Above 60 FPS is experimental: something may still run too fast. Tell us what and where.
- The keyboard and mouse controls are experimental.
- The first time an effect appears, it may show up a few frames late.
- The Xenos renderer has been tested less on Vulkan than on DirectX 12.


MORE INFORMATION

{MORE_INFO}

KameoRecomp's own code is under the MIT license (LICENSE). It is built on the ReXGlue SDK and other
components under their own licenses: see THIRD_PARTY_NOTICES.md and the "licenses" folder.
