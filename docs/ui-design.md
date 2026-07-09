# UI reference

Visual reference for the OSP2 interface as drawn by `src/gui/Gui.cpp`: layout, fonts, themes, style metrics, and the glyph/sprite inventory. The architecture — the `UiState`/`UiActions` seam, draw structure, and presentation state — is in [ui.md](ui.md).

The window is a fixed **1280×720** (Nintendo Switch resolution); the layout never resizes, so every metric below is expressed against that fixed viewport.

## Fonts

Loaded by `Platform::loadFonts`, all at **22 px**, merged into one default font:

| Font | File (`romfs/font/`) | Role |
|------|----------------------|------|
| Roboto Regular | `Roboto-Regular.ttf` | base text font, added first (authoritative over overlapping Latin) |
| Material Symbols Sharp Filled | `MaterialSymbolsSharp_Filled-Regular.ttf` | icon glyphs, merged (`MergeMode`, `GlyphOffset.y = 2`, glyph range `0x0030–0xFFCB`) |
| Noto Sans JP subset | `NotoSansJP-Subset.ttf` | CJK metadata (e.g. Shift-JIS NSF titles), merged with `GetGlyphRangesJapanese()` |

The Noto Sans JP file is pre-subset to kana + ~2999 common kanji by `scripts/gen_cjk_subset.py`; coverage is whatever the TTF holds (ImGui 1.92 rasterizes on demand), so widen it by regenerating the subset, not via the range argument.

## Overall layout — WORKSPACE mode

```
┌──────────────────────────────────────────────────────────────────────┐
│ OSP2 │ ⚙ Settings   ℹ About   ⏻ Quit                              ⛶ │  top bar (menu bar)
├────────────────────────────────┬─────────────────────────────────────┤
│ 📂 /music/mods                 │  Metadata │ Playlist                │
│ ────────────────────────────── │ ─────────────────────────────────── │
│ Name             Type    Size  │  Title      Cool Song               │
│ 📁 ..                          │  Format     S3M                     │
│ 📁 demos         Folder        │  Channels   16                      │
│ 🎵 cool.s3m      S3M   123 KB  │  ...                                │
│ 🎵 tune.mod      MOD    45 KB  │                                     │
│          (scrollable)          │                                     │
│                                │                                     │
├────────────────────────────────┴─────────────────────────────────────┤
│ 🎵 Cool Song · cool.s3m                                   Track 2/12 │
│ 0:42 ━━━━━━━●────────────────────────────────────────────────── 3:37 │  player bar (140 px)
│                        ⏮      ⏯      ⏹      ⏭                       │
└──────────────────────────────────────────────────────────────────────┘
```

Three stacked regions inside the ImGui fullscreen window:

1. **Top bar** — an ImGui main menu bar, drawn every frame in both view modes.
2. **Work area** — left pane (file browser) + right pane (tabs), side by side, filling the height between the top bar and the player bar. Drawn only in WORKSPACE mode.
3. **Player bar** — full-width strip pinned to the bottom, fixed height. Drawn only in WORKSPACE mode.

## Overall layout — VISUALIZATION mode

```
┌──────────────────────────────────────────────────────────────────────┐
│ OSP2 │ ⚙ Settings   ℹ About   ⏻ Quit                              ⛶ │  top bar only
├──────────────────────────────────────────────────────────────────────┤
│                                                                      │
│                                                                      │
│                  (active visualizer renders here)                    │
│                                                                      │
│                                                                      │
└──────────────────────────────────────────────────────────────────────┘
```

In VISUALIZATION mode only the top bar is drawn; the area below it — the viewport work area minus the menu bar — is handed to the active visualizer via `onRenderVisualization` (see [visualization.md](visualization.md)). Audio playback is unaffected: the player keeps running while collapsed, only the transport UI is hidden.

The mode is toggled by the `⛶` icon at the far right of the top bar — presentation state on the `Gui` (`m_viewMode`), not application state.

## Region: top bar

```
 OSP2 │ ⚙ Settings   ℹ About   ⏻ Quit                                ⛶
 └ app title (+ separator)  └ menus / entries                └ view toggle
```

- **App title** — plain `OSP2` text followed by a separator, non-interactive.
- **Settings menu** — a dropdown with three submenus, in this order:

  ```
  ⚙ Settings ▾
  ┌──────────────┐
  │ Theme      ▸ │  Dark / Light / Classic — checkmark on the active theme
  │ Visualizer ▸ │  one entry per visualizer plugin — checkmark on the active one
  │ Plugins    ▸ │  one entry per plugin that publishes settings
  └──────────────┘
  ```

  Theme selection applies immediately and is persisted to the INI. The Plugins submenu shows a dimmed *"No configurable plugins"* when no plugin publishes settings; picking a plugin opens its settings modal.
- **About** — opens the About modal.
- **Quit** — opens a confirm modal (*"Are you sure you want to quit?"*, **Quit** / **Cancel**). On desktop quitting exits like Esc / window-close; on Switch it returns to the Home menu.
- **View-mode toggle** — far right; flips between WORKSPACE and VISUALIZATION. Glyph is `fullscreen` in WORKSPACE and `fullscreen_exit` in VISUALIZATION.

### Modals

All modals are centered, auto-resizing, and opened from the menu-bar scope (so they work in both view modes):

- **About** — k7 cassette logo (256×171) on the left; on the right the app name, a `Version <OSP_VERSION> (<git rev>)` line, copyright + `GPL-3.0-or-later`, the project URL, and a "Powered by" credits block (decoder libraries, Dear ImGui, SDL2, fonts). **Close** button.

  ```
        ┌──────────────────────────────────────────────┐
        │             OSP2 — chiptune player           │
        │  [ k7 ]     Version 2.0.0 (abc1234)          │
        │  [logo ]    Copyright (C) 2026 ...           │
        │             GPL-3.0-or-later                 │
        │             Powered by: libopenmpt, ...      │
        │  ──────────────────────────────────────────  │
        │  [ Close ]                                   │
        └──────────────────────────────────────────────┘
  ```
- **Quit confirm** — one text line, **Quit** + **Cancel** buttons.
- **Playback error** — the error message and a **Close** button; shown when a track fails to load (see [ui.md](ui.md)).
- **Plugin settings** — one modal per configurable plugin, titled by the plugin name: one widget per setting descriptor (`SliderInt` for an int range, `Combo` for enum options), an optional dimmed *"Applies on the next track"* hint per row, then **Save** (persist to INI and close) and **Close** (keep live-applied values for the session only).

## Region: left pane — file browser

**45%** of the work-area width, full height between the top and player bars.

```
 📂 /music/mods                       ← current path header (accent folder glyph + path)
 ──────────────────────────────
 Name                Type     Size    ← table header row (frozen)
 📁 ..                                ← parent entry (hidden at the virtual root)
 📁 demos            Folder
 🎵 cool.s3m         S3M    123 KB    ← file rows: glyph, uppercase extension, size
 🎵 tune.mod         MOD     45 KB
          (scroll)
```

- **Path header** — folder glyph tinted with the amber accent, then the current path string.
- **Browser table** — three columns (`Name` stretch, `Type` fixed, `Size` fixed), scrollable body with a frozen header, `ImGuiListClipper` over the entries, row-background striping. `Type` shows `Folder`, `Source`, or the uppercase extension; `Size` shows B/KB/MB (one decimal) for files, right-aligned, blank for folders. Directories use the amber folder glyph; files use the blue file glyph. The `..` parent entry is pinned at the top inside a source and hidden at the virtual root (sources list). The currently-playing track's row is drawn highlighted (selected). File rows carry a right-click *Add to playlist* context menu.
- **Loading overlay** — while work is in flight the table is disabled and a dimmed overlay (background alpha 0.8) covers exactly the pane: a centered ASCII spinner (`| / - \`, ~8 steps/s, in a fixed-width slot so the label never jitters) beside the working label (`Scanning...` / `Downloading...` / `Loading...`), and a centered **Cancel** button below, focused once when the overlay appears so it is gamepad-reachable on the Switch.

## Region: right pane — tabs

Remaining width (~55%), full height between the top and player bars. A tab bar with exactly two tabs:

- **Metadata** — key/value table of the current track, with fields per decoder (module / GME / SID / sc68 layouts) and a scrollable message/comment block when present; a centered dimmed *"No track loaded"* when nothing is loaded.
- **Playlist** — **Shuffle** and **Repeat** checkboxes on top (always visible), then one row per entry: a blue checkbox glyph (`check_box` filled on the playing row, `check_box_outline_blank` hollow otherwise) + the entry name, highlighted on the playing row. Left-click plays the entry; right-click opens *Remove from playlist*. An empty playlist shows a centered dimmed *"Playlist is empty"*.

## Region: player bar

Full width, pinned to the bottom, fixed **140 px** height, window padding `12, 6`. The three rows (track line, progress, transport) are vertically centered as a block.

```
 🎵 Cool Song · cool.s3m                                       Track 2/12  ← track line
 0:42 ━━━━━━━●──────────────────────────────────────────────────── 3:37    ← progress row
                          ⏮       ⏯       ⏹       ⏭                       ← transport
```

- **Track line** — music-note glyph then `{title} · {filename}` from `UiState::status`; just the filename when the title is empty; `No track` when stopped. When the loaded file has more than one subtrack, a right-aligned `Track n/N` indicator (1-based) sits on the same line — clamped so a long title pushes it right rather than overlapping. Single-track files show no indicator.
- **Progress row** — `m:ss` position on the left, a display-only progress track in the middle, `m:ss` duration on the right (`--:--` when the duration is unknown, i.e. ≤ 0). The track is drawn by hand on the window draw list: a 3 px `FrameBg` line with rounded caps; while a track is loaded, the played portion is filled in the `PlotHistogram` accent up to a 6 px-radius circular playhead whose travel is inset by its radius. When stopped the row is just the empty line. Seeking is not supported — the bar is not interactive.
- **Transport** — four **48×48** `ImageButton`s from the sprite atlas, centered as a group: previous, play/pause, stop, next. The play/pause button shows the `pause` sprite while playing, else `play`. Button frame padding is 4 px.

### Player-bar states

| State | Track line | Progress | Play/pause sprite |
|-------|------------|----------|-------------------|
| Stopped | `No track` | `0:00` — empty line, no knob — `0:00` | `play` |
| Playing | `🎵 Cool Song · cool.s3m` | advances, `m:ss` both sides | `pause` |
| Paused | `🎵 Cool Song · cool.s3m` | frozen at current position | `play` |
| Unknown duration | as playing | `m:ss` — unfilled line — `--:--` | as playing |

## Themes

Colors come entirely from ImGui's three built-in palettes — no custom palette. `applyTheme(theme)` maps each `Theme` enumerator to the matching ImGui call:

| `Theme` | ImGui call | Look |
|---------|------------|------|
| `DARK` | `ImGui::StyleColorsDark` | dark grey — the default |
| `LIGHT` | `ImGui::StyleColorsLight` | light grey / white |
| `CLASSIC` | `ImGui::StyleColorsClassic` | the legacy translucent-blue theme |

Each `StyleColorsXxx` writes only `ImGuiStyle::Colors[]`, so it can be called live from the Theme submenu without touching layout. The choice is persisted to the INI and restored by `Platform` at startup (unknown values fall back to dark).

**Fixed accent colors** — the only colors not driven by the theme, kept so file kinds stay distinguishable in every theme:

| Accent | RGBA | Use |
|--------|------|-----|
| amber `0.90, 0.70, 0.20` | folder glyphs (path header, directory rows, `..`) |
| blue `0.20, 0.60, 0.90` | file glyphs (browser rows) and playlist checkbox glyphs |

The progress fill and playhead use the theme's `PlotHistogram` color.

## Shared style metrics

Theme-independent size vars, set **once in `Gui::initialize()`**; `applyTheme` never re-applies them.

| Style var | Value | Notes |
|-----------|-------|-------|
| `WindowRounding` | 0 | fullscreen window has no rounded corners |
| `ChildRounding` | 6 | panes / player-bar child |
| `FrameRounding` | 6 | buttons, inputs, sliders |
| `PopupRounding` | 6 | modals, menus |
| `GrabRounding` | 6 | slider/scrollbar grabs |
| `TabRounding` | 6 | |
| `ScrollbarRounding` | 6 | |
| `WindowPadding` | 12, 12 | |
| `FramePadding` | 10, 8 | |
| `ItemSpacing` | 10, 8 | |
| `ItemInnerSpacing` | 8, 6 | |
| `ScrollbarSize` | 14 | |
| `GrabMinSize` | 12 | |
| `WindowBorderSize` | 0 | flat fullscreen window |
| `ChildBorderSize` | 1 | subtle pane separation |
| `FrameBorderSize` | 0 | |

### Layout metrics (fixed 1280×720)

| Metric | Value |
|--------|-------|
| Menu bar height | ImGui-computed (~30 px) |
| Player bar height | 140 px |
| Left pane width | 45% of work-area width |
| Right pane width | remaining work-area width |
| Transport button size | 48 × 48 px |
| Transport sprite source | 64 × 64 (scaled down to 48) |

## Glyph inventory

Material Symbols codepoints used in `Gui.cpp` labels (merged font, raw UTF-8 in string literals):

| Name | Codepoint | Use |
|------|-----------|-----|
| settings | `U+E8B8` | Settings menu label |
| info | `U+E88E` | About entry |
| power_settings_new | `U+E8AC` | Quit entry |
| fullscreen | `U+E5D0` | view toggle, WORKSPACE → VISUALIZATION |
| fullscreen_exit | `U+E5D1` | view toggle, VISUALIZATION → WORKSPACE |
| folder | `U+E2C7` | path header + directory/`..` rows (amber accent) |
| audio_file | `U+EB82` | file rows (blue accent) |
| music_note | `U+E405` | player-bar track line |
| check_box | `U+E834` | playlist: playing row |
| check_box_outline_blank | `U+E835` | playlist: idle rows |

Menu/label icons follow an **icon + single space + text** convention.

## Sprite atlas

`romfs/sprites/sprites.png`, keys parsed from `sprites.bin` (see [ui.md](ui.md) for the binary format; regenerate with the `make-spritesheet` skill):

| Key | Size (px) | Use |
|-----|-----------|-----|
| `play` | 64 × 64 | play/pause button when stopped/paused |
| `pause` | 64 × 64 | play/pause button when playing |
| `stop` | 64 × 64 | stop button |
| `previous` | 64 × 64 | previous button |
| `next` | 64 × 64 | next button |
| `k7` | 256 × 171 | About modal cassette logo |

## Time formatting

Position and duration are formatted `m:ss` (minutes, zero-padded seconds) by a local helper in `Gui.cpp` — e.g. `42.0 → "0:42"`, `217.0 → "3:37"`. Negative or NaN inputs clamp to `0:00`; an unknown duration (≤ 0) is displayed as `--:--` instead.
