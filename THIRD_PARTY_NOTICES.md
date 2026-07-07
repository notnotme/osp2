# Third-party notices

OSP2 is licensed under the GPL-3.0-or-later (see `LICENSE`). It includes or links against the following third-party software:

## Libraries

- **SDL2** — zlib License — https://www.libsdl.org/
- **SDL2_image** — zlib License — https://github.com/libsdl-org/SDL_image
- **Dear ImGui** (git submodule, `external/imgui`) — MIT License, Copyright (c) 2014-2026 Omar Cornut — https://github.com/ocornut/imgui — license text in `external/imgui/LICENSE.txt`. Bundles the stb libraries (public domain / MIT).
- **glad** (OpenGL loader) — MIT License, Copyright (c) 2013-2021 David Herberth — https://github.com/Dav1dde/glad
- **libopenmpt** — BSD-3-Clause License — https://lib.openmpt.org/
- **libgme** (game-music-emu) — LGPL-2.1-or-later; the optional MAME YM2612 emulator core, if built in, makes that configuration GPL-2.0-or-later — https://github.com/libgme/game-music-emu
- **libsidplayfp** — GPL-2.0-or-later — https://github.com/libsidplayfp/libsidplayfp
- **libresidfp** (reSID-fp SID emulation engine, linked via libsidplayfp) — GPL-2.0-or-later — https://github.com/libsidplayfp/libresidfp
- **sc68** (`libsc68` + `file68` + `unice68`) — GPL-3.0-or-later, Copyright (c) 1998-2016 Benjamin Gerard — https://sourceforge.net/projects/sc68/
- **libcurl** — curl License (a permissive MIT/X-derived license; SPDX identifier `curl`) — https://curl.se/

## Data (`romfs/`)

- **HVSC Songlengths database** (`romfs/sidlength/Songlengths.md5`) — from the **High Voltage SID Collection (HVSC)**, compiled by the HVSC crew; freely redistributable per the HVSC terms — https://www.hvsc.c64.org/ . Maintainer-supplied and `.gitignore`d (not in the repo); only baked into a build when present. Used to look up real SID track lengths (see [docs/audio.md](docs/audio.md)).

## Fonts (`romfs/font/`)

- **Roboto** — SIL Open Font License 1.1, Copyright 2011 The Roboto Project Authors — license text in `romfs/font/OFL.txt`
- **Material Symbols** (Sharp Filled) — Apache License 2.0 — https://github.com/google/material-design-icons — license text in `romfs/font/LICENSE.txt`
- **Noto Sans JP** (`NotoSansJP-Subset.ttf`, subset to Dear ImGui's Japanese glyph ranges) — SIL Open Font License 1.1, Copyright 2014-2021 Adobe (http://www.adobe.com/), with Reserved Font Name "Source" — https://fonts.google.com/noto/specimen/Noto+Sans+JP — license text in `romfs/font/OFL-NotoSansJP.txt`
