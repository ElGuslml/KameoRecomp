# Game files

Put the files of your own copy of **Kameo: Elements of Power (Xbox 360)** here, so that this file exists:

```
assets/default.xex
```

The whole disc content goes in this folder (`default.xex`, the `*.lvl` and `*.mdl` files and the `audio`,
`fmv` and language folders). Extract it from your disc or ISO with
[extract-xiso](https://github.com/XboxDev/extract-xiso/releases).

The build needs `default.xex` (the code is recompiled from it); the game needs everything else.
The expected executable has CRC32 `E24BE720` (3,682,304 bytes, title ID `4D5307D2`); `KameoEOPRecomp.exe`
warns at startup if yours is different.

Nothing in this folder is tracked by git.
