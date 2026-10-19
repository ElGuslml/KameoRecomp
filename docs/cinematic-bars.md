# Cinematics without black bars

**Status: working (2026-10-19).** Tested in the first cinematic of Thorn's Castle at 16:9 and in ultrawide, with both
renderers.

## Symptom

The in-engine cinematics show black bars above and below the image (each 1/8 of the screen's height, so the image
is about 2.37:1), also on an ultrawide screen, where they leave only the middle of the screen for the image.

## Cause

The bars are not part of the scene: the game renders the whole 1280×720 frame and then draws two black rectangles
over it (`sub_822712E0`, through the rectangle fill `sub_822D19E0`), their height animated when a cinematic starts
and ends.

## Fix

`src/cinematics.cpp` with two `[[midasm_hook]]`s in `kameo_functions.toml`, active only with `kameo_no_letterbox`
(Kameo Launcher: **Cinematics: No black bars**): they skip the two calls that draw the bars. The rest of the code,
which animates the bars, runs as before, so the cinematics keep their timing. It works at 16:9 and with
[ultrawide](ultrawide.md).

With the bars kept and ultrawide on, the bars span the whole width of the screen (they are full-width rectangles,
which the ultrawide fix leaves at full width).

## Turning it off

Uncheck **Cinematics: No black bars** in Kameo Launcher (it writes `kameo_no_letterbox = false` to
`kameo_options.toml`) or start the game with `--kameo_no_letterbox=false`.

| Option (`kameo_options.toml`, `kameo.toml` or `--name=value`) | Default | Meaning |
|---|---|---|
| `kameo_no_letterbox` | `false` | `true`: the in-engine cinematics are drawn without their black bars |

## What to watch for

Something at the top or bottom of a cinematic that the bars used to hide (an object cut off, the edge of the set),
or subtitles that are hard to read over the image.
