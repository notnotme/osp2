# TODO_11 — Back-fill `THIRD_PARTY_NOTICES.md`

> Priority, docs-only, single chunk. The notices file lists only `libopenmpt` of the four decoder libraries now linked, and omits `libcurl` — a compliance gap left by TODO_7 + TODO_9, which added the dependencies but never updated the notice. No code, build, or behaviour change.

## Context

`THIRD_PARTY_NOTICES.md` (repo root) currently credits SDL2, SDL2_image, Dear ImGui, glad, **libopenmpt**, and the bundled fonts (Roboto, Material Symbols) under `## Libraries`, one bullet each: `- **name** — License — url`.

`CMakeLists.txt:64–111` links, in addition to the above:

- **libgme** (game-music-emu) — `GmePlugin` — LGPL-2.1-or-later (with permissively-licensed parts) — <https://bitbucket.org/mpyne/game-music-emu>
- **libsidplayfp** (+ **libresidfp**) — `SidPlugin` — GPL-2.0-or-later — <https://github.com/libsidplayfp/libsidplayfp>
- **libsc68** / **file68** / **unice68** (sc68) — `Sc68Plugin` — GPL-3.0-or-later — <https://sourceforge.net/projects/sc68/>
- **libcurl** — `FtpDataSource` / Modland remote source — curl license (MIT/X derivative) — <https://curl.se/>

All four are missing from the notice. Confirm exact upstream license names/URLs against each project before writing (the SPDX identifiers above are the starting point, not gospel — verify libgme's split licensing and sc68's exact GPL version).

## Task chunks (implement, verify, and commit one at a time)

- [ ] **11a — Back-fill the four missing dependencies**: add `libgme`, `libsidplayfp` (+ `libresidfp`), `libsc68`/`file68`/`unice68`, and `libcurl` to the `## Libraries` list in `THIRD_PARTY_NOTICES.md`, each with its verified upstream license and URL, matching the existing bullet format. Cross-check the final list against the `target_link_libraries` / `pkg_check_modules` stanzas in `CMakeLists.txt:64–111` so every linked third party is credited. Verify: every library in CMakeLists appears in the notice with a plausible license; desktop + Switch builds still green (unchanged — pure doc).

This chunk ends with green desktop + Switch builds (unchanged), the notice updated, user verification, then a commit.

## Files to change

1. **`THIRD_PARTY_NOTICES.md`** — add the four missing entries under `## Libraries`.

No source or `CMakeLists.txt` change.

## Docs

- None beyond the notice file itself (`THIRD_PARTY_NOTICES.md` is the deliverable).

## Coordination

- Independent. Documents dependencies introduced by **TODO_7** (libcurl) and **TODO_9** (libgme/libsidplayfp/libsc68); no dependency on other backlog items.

## Verification

- Desktop + Switch builds green (per CLAUDE.md) — unaffected by a doc change, but confirm.
- Diff the `## Libraries` list against `CMakeLists.txt:64–111`: every linked third-party library is credited with a license and URL; no linked lib is missing.
