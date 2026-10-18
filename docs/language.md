# Game language

**Status: working (2026-10-12).** Checked at the main menu (English, both Spanish versions, Korean) and in
the opening scene (Korean subtitles).

## Symptom

The game always started in English, and there was no way to get the Latin American Spanish that is on the
disc.

## Cause

Kameo has no language menu: at start-up it asks the console for the dashboard language (`XGetLanguage`) and
loads the matching folder of the game files (`english`, `french`, `german`, `italian`, `spanish`,
`japanese`, `korean`, `tchinese`). The `mexican` folder (Latin American Spanish) is used for Spanish only on
a North American console (`XGetGameRegion` = `0xFF`). ReXGlue answers `XGetLanguage` with the
`user_language` setting (SDK patch 0007; English by default) and reports all regions (`0xFFFF`) as the game
region, so Spanish was always the Spanish of Spain.

## Fix

- Kameo Launcher has a **Language** row (it writes `user_language` to `kameo_options.toml`). The first time,
  it picks the Windows display language; the launcher's own texts follow the chosen language.
- For **Español (Latinoamérica)** it also writes `kameo_latin_american_spanish = true`: a
  `[[midasm_hook]]` (`src/language.cpp`) reports the North American region to the game's own check when the
  language is Spanish, and the game loads the `mexican` folder.

Korean and Traditional Chinese have text but no voice files on the disc: the game looks for them in its
language folder and does not find them.

## Options

| Option (`kameo_options.toml`, `kameo.toml` or `--name=value`) | Default | Meaning |
|---|---|---|
| `user_language` | `1` | 1 English, 2 Japanese, 3 German, 4 French, 5 Spanish, 6 Italian, 7 Korean, 8 Traditional Chinese |
| `kameo_latin_american_spanish` | `false` | With `user_language = 5`: `true` = Latin American Spanish, `false` = Spanish of Spain |
