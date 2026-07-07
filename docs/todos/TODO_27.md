# TODO_27 — Reorder the Settings menu → [Theme][Visualizer][Plugins]

> The Settings menu lists its submenus as Theme, Plugins, Visualizer. Reorder to Theme, Visualizer, Plugins. Single chunk, presentation-only.

## Context

The `" Settings"` menu is built with nested `ImGui::BeginMenu` submenus in `src/gui/Gui.cpp` (outer `BeginMenu(" Settings")` at `:219`):

- **Theme** submenu — `:220-234` (Dark / Light / Classic `MenuItem`s)
- **Plugins** submenu — `:236-253` (one `MenuItem` per plugin that has settings)
- **Visualizer** submenu — `:258-265` (one `MenuItem` per visualizer, check on the active one)

Desired order: **Theme, Visualizer, Plugins**.

## The fix (single chunk)

Move the self-contained Visualizer `BeginMenu`/`EndMenu` block (comment `:255-257` + block `:258-265`) to sit between the Theme block (ends `:234`) and the Plugins block (starts `:236`). Pure reordering of two adjacent submenu blocks — no behavior change.

## Files to change

1. **`src/gui/Gui.cpp`** — reorder the submenu blocks in the Settings menu (`:219-265`).

## Docs

- **`docs/ui.md`** — update the Settings-menu order if it's described there.

## Verification

- Desktop + Switch builds green.
- The Settings menu shows Theme, then Visualizer, then Plugins; every submenu item still works (theme switch, visualizer select with active checkmark, plugin settings popups).
