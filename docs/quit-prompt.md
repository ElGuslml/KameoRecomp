# Quitting from the title screen

**Status: working (2026-10-21).** Tested on the title screen with both renderers (NativeXRenderer on Vulkan, Xenos on
Direct3D 12), with the keyboard as the controller.

## Symptom

The game has no way to quit it: on the Xbox 360 that was the console's guide button. On PC the only way out was to
close the window.

## Fix

On the title screen ("PRESS START"), **B** now asks whether to quit, in the game's language: **A** (or Enter) quits,
**B** (or Escape) goes back to the title screen. The mouse works too. The dialog is not drawn with the game's
graphics: it is a window over the dimmed game, like the SDK's other dialogs. The game does not see the button
presses that open, answer or close it.

A B pressed in the main menu still goes back to the title screen as before: only a new press of B on the title
screen opens the dialog.

The dialog belongs to the SDK (`rex/ui/overlay/quit_prompt.h`, a change on its `recomp` branch), so other games can
use it: each game only says when it shows a screen that can be quit from. In Kameo that is
`src/quit_prompt.cpp`, a `[[midasm_hook]]` in the code that runs the title sequence (`sub_82414408`, whose state
8 is the title screen).

## Turning it off

Start the game with `--quit_prompt=false`, or add `quit_prompt = false` to `kameo.toml`. B on the title screen
then does what it always did (nothing).

| Option (`kameo.toml` or `--name=value`) | Default | Meaning |
|---|---|---|
| `quit_prompt` | `true` | `true`: B on the title screen asks whether to quit the game |

## What to watch for

The dialog not opening, a B or A press that reaches the game after the dialog closes, or a controller that stops
responding after it.
