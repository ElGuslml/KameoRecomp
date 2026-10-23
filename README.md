<p align="center">
  <img src="res/icon.svg" width="200" alt="KameoRecomp icon">
</p>

<h1 align="center">KameoRecomp</h1>

<p align="center">
  <a href="https://github.com/ElGuslml/KameoRecomp/releases/latest"><img alt="Latest release" src="https://img.shields.io/github/v/release/ElGuslml/KameoRecomp?style=for-the-badge&label=Latest%20release&color=86a8ff"></a>
  <a href="https://discord.gg/XQtM5TmNs8"><img alt="Discord: join the server" src="https://img.shields.io/badge/Discord-Join%20the%20server-5865F2?style=for-the-badge&logo=discord&logoColor=white"></a>
  <a href="https://github.com/ElGuslml/ElGusRecomps"><img alt="More games: ElGus Recomps" src="https://img.shields.io/badge/More%20games-ElGus%20Recomps-2f6b45?style=for-the-badge&logo=github"></a>
</p>

🧚 **Play Kameo: Elements of Power on your PC, natively.** The Xbox 360 classic by Rare (2005), recompiled to run
directly on Windows: no emulator to set up, sharper graphics, wide screens and higher frame rates.

> [!IMPORTANT]
> 💿 **You need your own original copy of the game.** This project does not include any of the game's files, and
> never will.
> - To play, you need an Xbox 360 copy of *Kameo: Elements of Power* that you own (the USA disc).
> - You make an image (ISO) of your own disc, and the launcher installs the game from it.
> - Please do not ask for the game's files, and do not share them or links to them.
>
> This project is not affiliated with or endorsed by Microsoft or Rare.

> [!WARNING]
> 🚧 **Early release.** Thorn's Castle and the first areas of the Enchanted Kingdom have been played without
> problems; later levels have not been tested yet. Please [tell us](#-reporting-a-problem) what you find!

## 🎮 How to play

1. **Download** the latest version from [Releases](https://github.com/ElGuslml/KameoRecomp/releases/latest):
   - **the installer** (`KameoRecomp-...-setup.exe`): installs for your Windows user, no administrator rights
     needed, with a Start menu shortcut; or
   - **the zip** (`KameoRecomp-win64.zip`): extract it into any folder you can write to (not `Program Files`).
2. **Open Kameo Launcher.** The first time, it asks for the ISO of your game, checks it, and installs the game
   files (about 5.4 GB). You only do this once.
3. **Pick your settings and press PLAY.** 🎉

From then on, the launcher keeps itself up to date: when a new version comes out, it offers to install it for you.

### 💻 What you need

- Windows 10 or 11 (64-bit).
- A graphics card with up-to-date drivers that support Vulkan (practically any card from the last ten years).
- The [Microsoft Visual C++ Redistributable](https://aka.ms/vs/17/release/vc_redist.x64.exe) (most PCs already
  have it).
- An Xbox controller, a PlayStation controller, or any other controller. Keyboard and mouse work too
  (experimental).

## ✨ Features

- 🖥️ **Sharp, native graphics:** the game is drawn by your graphics card at your screen's resolution, with
  smooth edges (antialiasing), sharper textures and higher-quality shadows.
- ⚡ **60 FPS and more:** play at 60 frames per second at the original game speed, or from 120 up to 240 FPS if
  your monitor supports it (experimental).
- 📺 **Wide and ultrawide screens:** the game fills 16:9, 21:9 and wider screens and shows more of the world to
  the sides, while the menus and videos keep their shape.
- 🎬 **Cinematics without black bars** (optional).
- 🏆 **Achievements:** a notification when you unlock one, and the full list in the launcher or with F7 in the
  game.
- 🌍 **Languages:** English, French, German, Italian, Spanish (from Spain or Latin America), Japanese, Korean
  and Traditional Chinese.
- 🕹️ **Your controls:** change the buttons of your controller, or play with the keyboard and mouse.
- 🔄 **Automatic updates** from this page.

## 🧭 Kameo Launcher

Everything is set up from **Kameo Launcher**, with the mouse, the keyboard or a controller:

| Page | What you find there |
|---|---|
| ▶️ **Play** | Start the game, and see your settings, achievements and updates at a glance. |
| 🏆 **Achievements** | All 26 achievements, the ones you have unlocked and when. |
| 🎮 **Controls** | Keyboard and mouse (experimental), and your controller's buttons. |
| 🖥️ **Video** | Monitor, fullscreen or window, resolution, screen shape (fill a wide screen or keep 16:9). |
| 🎨 **Graphics** | Renderer, frame rate, antialiasing, texture sharpness, shadow quality. |
| 🌍 **Game** | The game's language, and the cinematics without black bars. |
| 🔄 **Updates** | Your version, and new ones when they come out. |

Your choices are saved as soon as you make them. When you quit the game, the launcher comes back.

**Tips:**
- 🚪 On the title screen ("PRESS START"), press **B** to quit the game.
- 🎮 **Steam Big Picture:** add `Kameo Launcher.exe` as a non-Steam game and play from your couch.
- 🛡️ **"Windows protected your PC":** the programs are not signed by a company, so Windows may warn you the first
  time. Click **More info**, then **Run anyway**.

## 🐞 Reporting a problem

Something looks wrong, runs too fast, or crashes? Tell us on [Discord](https://discord.gg/XQtM5TmNs8) or open an
[issue](https://github.com/ElGuslml/KameoRecomp/issues), with:
- where in the game it happened, and a screenshot if you can;
- the latest file in the `logs` folder next to the game.

If something looks wrong, try the other renderer: in the launcher, **Graphics → Xenos** (it emulates the Xbox 360's
graphics instead), and tell us whether it happens there too.

### Known issues

- ⚡ Above 60 FPS, something may still run too fast.
- ⌨️ Keyboard and mouse controls are experimental.
- ✨ The first time an effect appears, it may show up a moment late.

## 🛠️ For developers

KameoRecomp is a *static recompilation*: the game's original Xbox 360 code is translated to C++ and compiled for
Windows, on top of the [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk). It is drawn by
[NativeXRenderer](https://github.com/ElGuslml/NativeXRenderer), a renderer that draws the game's graphics calls
natively on Vulkan.

- [Building and developing](docs/development.md): requirements, building from source, configuration, how the SDK is
  modified.
- [`docs/`](docs/README.md): a short note on each problem found and how it was solved.

## 🙏 Credits

- [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk), whose runtime is derived from [Xenia](https://xenia.jp).
- [extract-xiso](https://github.com/XboxDev/extract-xiso), for extracting game files.
- Margen67 (60 FPS) and ISpectre23 ([FPS Camera Fix](https://github.com/ISpectre23/Kameo-FPS-Camera-Fix)), whose
  [Xenia Canary game patches](https://github.com/xenia-canary/game-patches) for Kameo showed what to change for
  60 FPS.
- Developed by Gustavo Garcia with the help of Claude (Anthropic).

## 📜 License

MIT for this project's own code (see [`LICENSE`](LICENSE)). The ReXGlue SDK and the other components keep their
own licenses (see [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)). Kameo: Elements of Power, its code and its
assets belong to their respective owners.
