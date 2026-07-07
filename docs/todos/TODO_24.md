# TODO_24 — Better About box

> The About box shows only a logo, "OSP2 — chiptune player", and "GPL-3.0-or-later". Enrich it with **version + build**, **author + copyright**, **decoder-library credits**, and a **project link**. Single chunk.

## Context

`Gui::drawAboutPopup()` (`src/gui/Gui.cpp:322-341`) is a centered auto-resize modal (`BeginPopupModal("About", ...)`, `:327`) opened from the `" About"` menu item (`:270-272` sets `m_aboutRequested`; `:290-294` opens the popup). It currently draws the `k7` sprite logo (`ImGui::Image`, `:328-329`), `"OSP2 — chiptune player"`, `"GPL-3.0-or-later"`, and a Close button. No version, author, credits, or link.

There is **no app version constant** anywhere today (the only "version" text in the tree is GPL boilerplate). `THIRD_PARTY_NOTICES.md` already lists the decoder libraries and fonts and their licenses.

## The fix (single chunk)

Add to the modal, keeping the existing centered-modal idiom and `ImGui::Image` logo:

- **Version + build**: introduce an app version — prefer CMake `project(OSP2 VERSION x.y.z)` surfaced as a compile definition (e.g. `-DOSP_VERSION="..."`), or a constant in `src/Paths.h`. Optionally include a build/commit string (also via a compile definition) — keep optional so it degrades cleanly when unset.
- **Author + copyright**: "Copyright (C) 2026 Romain Graillot", "GPL-3.0-or-later".
- **Decoder credits**: a short block crediting libopenmpt, libgme, libsidplayfp, sc68 (+ the bundled fonts), mirroring `THIRD_PARTY_NOTICES.md`. Versions optional.
- **Project link**: the source/homepage URL as plain selectable text (ImGui has no clickable-link helper here; a clickable link is a possible tiny follow-up, not required).

## Files to change

1. **`src/gui/Gui.cpp`** — `drawAboutPopup` (`:322-341`) content.
2. **`CMakeLists.txt`** (or **`src/Paths.h`**) — define the version (and optional build) constant for both desktop and Switch builds.

## Docs

- **`docs/ui.md`** — note the About box contents and where the version constant comes from.

## Coordination

- Independent. Touches `Gui.cpp` (serialise with the other UI items) and `CMakeLists.txt` — keep desktop + Switch config paths in sync.

## Verification

- Desktop + Switch builds green; version string resolves on both.
- About box shows version+build, author+copyright, decoder credits, and the project link, and still opens/closes from the menu.
