# UI design

The design source of truth for the OSP2 interface redesign (TODO_3). ASCII mockups,
theme choices, style metrics, and the glyph inventory here are the spec that
`src/gui/Gui.{h,cpp}` implements. When the implementation and this document disagree,
this document is what the user reviewed — reconcile the code to it (or update this doc
in the same change if the design itself moved).

The window is a fixed **1280×720** (Nintendo Switch resolution); the layout never
resizes, so every metric below is expressed against that fixed viewport. Fonts are
**Roboto 22 px** with **Material Symbols (Sharp, Filled) 22 px** merged in for the
icon glyphs.

## Overall layout — WORKSPACE mode

```
┌──────────────────────────────────────────────────────────────────────┐
│ OSP2                                      ⚙ Settings   About       ⛶ │  top bar (menu bar)
├────────────────────────────────┬─────────────────────────────────────┤
│ 📂 /music/mods                 │  Metadata │ Playlist                │
│ ────────────────────────────── │ ─────────────────────────────────── │
│ 📁 ..                          │  Title      Cool Song               │
│ 📁 demos                       │  Format     S3M                     │
│ 🎵 cool.s3m           123 KB   │  Channels   16                      │
│ 🎵 tune.mod            45 KB   │  ...                                │
│ 🎵 anthem.it           88 KB   │                                     │
│          (scrollable)          │                                     │
│                                │                                     │
├────────────────────────────────┴─────────────────────────────────────┤
│ 🎵 Cool Song · cool.s3m                                              │
│ 0:42 ━━━━━━━●────────────────────────────────────────────────── 3:37 │  player bar (140 px)
│                        ⏮      ⏯      ⏹      ⏭                       │
└──────────────────────────────────────────────────────────────────────┘
```

Three stacked regions inside the ImGui fullscreen window:

1. **Top bar** — an ImGui main menu bar, drawn every frame in both view modes.
2. **Work area** — left pane (file browser) + right pane (tabs), side by side, filling
   the height between the top bar and the player bar. Drawn only in WORKSPACE mode.
3. **Player bar** — full-width strip pinned to the bottom, fixed height. Drawn only in
   WORKSPACE mode.

## Overall layout — VISUALIZATION mode

```
┌──────────────────────────────────────────────────────────────────────┐
│ OSP2                                      ⚙ Settings   About       ⛶ │  top bar only
├──────────────────────────────────────────────────────────────────────┤
│                                                                      │
│                                                                      │
│                 (empty — reserved for visualizations)                │
│                                                                      │
│                                                                      │
└──────────────────────────────────────────────────────────────────────┘
```

In VISUALIZATION mode only the top bar is drawn. The fullscreen window (panes + player
bar) is skipped entirely, so the GL clear color shows through the whole area below the
menu bar. That area — **the viewport work area minus the menu bar height** — is the
region reserved for the future visualization system (TODO_8), which will render there
either as raw GL under ImGui or via a draw hook handed that rectangle. Audio playback is
unaffected by the mode: the player keeps running while collapsed; only the transport UI
is hidden.

The mode is toggled by the `⛶` icon button at the far right of the top bar. It is
presentation state on the `Gui` (`m_viewMode`), not application state — nothing in the
player or filesystem domains knows about it.

## Region: top bar

```
 OSP2                                        ⚙ Settings   About       ⛶
 └ app title                                 └ menu       └ menu      └ view toggle
```

- **App title** — plain `OSP2` text, left-aligned, non-interactive.
- **Settings menu** — a dropdown; theme selection lives in a **Theme** submenu:

  ```
  ⚙ Settings ▾
  ┌───────────┐
  │ Theme   ▸ │──┬─────────────┐
  └───────────┘  │ ● Dark      │   ← radio, checkmark on the active theme
                 │ ○ Light     │
                 │ ○ Classic   │
                 └─────────────┘
  ```

  Selecting an entry calls `applyTheme(theme)` immediately and updates `m_theme`
  (drives the checkmark). Persistence of the choice is TODO_6; for now it resets to the
  dark default on each launch. This menu is also where TODO_6's plugin-configuration UI
  will land.
- **About** — a menu/button that opens the About popup (below).
- **View-mode toggle** `⛶` — far right; flips `m_viewMode` between WORKSPACE and
  VISUALIZATION. Glyph is `fullscreen` when in WORKSPACE (▶ go fullscreen) and
  `fullscreen_exit` when in VISUALIZATION (◀ come back).

### About popup

A modal popup, centered, showing the k7 cassette logo (256×171) and a short credit line.

```
        ┌────────────────────────────┐
        │                            │
        │        [ k7 logo ]         │
        │                            │
        │   OSP2 — chiptune player   │
        │   GPL-3.0-or-later         │
        │                            │
        │                  [ Close ] │
        └────────────────────────────┘
```

## Region: left pane — file browser

~**45%** of the work-area width, full height between the top and player bars.

```
 📂 /music/mods                    ← current path header (accent folder glyph + path)
 ──────────────────────────────
 Name                     Size     ← table header row (frozen)
 📁 ..                             ← parent entry
 📁 demos                          ← directory rows (accent glyph)
 🎵 cool.s3m            123 KB     ← file rows (file glyph + size, right column)
 🎵 tune.mod             45 KB
          (scroll)
```

- **Path header** — folder glyph (`\ue2c7`) tinted with the accent color, then the
  current path string.
- **Browser table** — two columns (`Name` stretch, `Size` fixed), scrollable body with a
  frozen header, `ImGuiListClipper` over the entries (kept from the current
  implementation). Restyled: **no outer borders**, subtle **row-background striping**,
  **hover highlight** on rows. Directories use the accent folder glyph; files use the
  file glyph (`\ueb82`) and show their size in the right column. The `..` parent entry is
  always present at the top.
- **Loading overlay** — when `state.isWorking`, a translucent overlay with an
  indeterminate progress bar is drawn centered over the pane (kept from current behavior).
  TODO_4 wires this to the real threaded backend.

Clicking a file row invokes `onFileClick`. Directory navigation (`..`, subfolders) is
wired in TODO_4; here the rows are drawn but only file clicks act.

## Region: right pane — tabs

Remaining width (~55%), full height between the top and player bars. A tab bar with
exactly two tabs:

- **Metadata** — key/value view of the current track (Title, Format, Channels, …).
  Placeholder content until TODO_5 supplies typed per-plugin metadata. This absorbs the
  old standalone "track information" table.
- **Playlist** — placeholder for the upcoming playlist view.

The old **Settings** and **About** tabs are gone — Settings moved to the top-bar menu,
About to the top-bar popup.

## Region: player bar

Full width, pinned to the bottom, fixed **140 px** height. The three rows (track line,
progress, transport) are vertically centered as a block so the transport buttons keep a
clear margin above the bar's bottom edge.

```
 🎵 Cool Song · cool.s3m                                                    ← track line
 0:42 ━━━━━━━●──────────────────────────────────────────────────── 3:37     ← progress row
                          ⏮       ⏯       ⏹       ⏭                        ← transport
```

- **Track line** — music-note glyph then `"{title} · {filename}"` from
  `UiState::status`. When the title is empty, show just the filename; when stopped (no
  track), show `No track`.
- **Progress row** — `m:ss` position on the left, a **display-only** progress bar in the
  middle (fraction = `positionSeconds / durationSeconds`, clamped; empty when duration is
  0/unknown), `m:ss` duration on the right. Seeking is not supported (the decoder plugins
  expose no seek), so the bar is not interactive — this is future work.
- **Transport** — four `ImageButton`s from the sprite atlas, centered as a group:
  `⏮ previous`, `⏯ play/pause`, `⏹ stop`, `⏭ next`. The play/pause button shows the
  **`pause`** sprite while `state.status.state == PLAYING` and the **`play`** sprite
  otherwise. Buttons are **48×48** (down from 64×64) to fit the bar height.

### Player-bar states

| State    | Track line                    | Progress                          | Play/pause sprite |
|----------|-------------------------------|-----------------------------------|-------------------|
| Stopped  | `No track`                    | `0:00` ▁▁▁▁▁ `0:00` (empty)       | `play`            |
| Playing  | `🎵 Cool Song · cool.s3m`     | advances, `m:ss` both sides       | `pause`           |
| Paused   | `🎵 Cool Song · cool.s3m`     | frozen at current position        | `play`            |

## Themes

Colors come **entirely from ImGui's three built-in color themes** — no custom palette.
`applyTheme(theme)` maps each `Theme` enumerator to the matching ImGui call:

| `Theme`   | ImGui call                | Look                                  |
|-----------|---------------------------|---------------------------------------|
| `DARK`    | `ImGui::StyleColorsDark`  | dark grey, default — **the launch default** |
| `LIGHT`   | `ImGui::StyleColorsLight` | light grey / white                    |
| `CLASSIC` | `ImGui::StyleColorsClassic` | the legacy translucent-blue theme   |

Each `StyleColorsXxx` writes only `ImGuiStyle::Colors[]`, so it can be called live from
the Theme submenu to reskin every frame afterwards without touching layout. The **shared
style metrics** below (rounding, padding, …) are size vars, not colors — they are set
**once in `initialize()`** and survive theme switches, so `applyTheme` never needs to
re-apply them.

The two in-code `TextColored` folder/file glyph tints in the browser are the only colors
not driven by the built-in theme; they are kept as fixed accents (folder = amber
`0.90, 0.70, 0.20`, file = blue `0.20, 0.60, 0.90`) so directories and files stay
distinguishable in every theme.

## Shared style metrics

Theme-independent — set **once in `initialize()`** (before the first `applyTheme`), not per switch.

| Style var             | Value        | Notes                                    |
|-----------------------|--------------|------------------------------------------|
| `WindowRounding`      | 0            | fullscreen window has no rounded corners |
| `ChildRounding`       | 6            | panes / player-bar child                 |
| `FrameRounding`       | 6            | buttons, inputs, sliders                 |
| `PopupRounding`       | 6            | About popup, menus                       |
| `GrabRounding`        | 6            | slider/scrollbar grabs                   |
| `TabRounding`         | 6            |                                          |
| `ScrollbarRounding`   | 6            |                                          |
| `WindowPadding`       | 12, 12       |                                          |
| `FramePadding`        | 10, 8        | replaces the old ad-hoc `8,8`            |
| `ItemSpacing`         | 10, 8        | generous spacing                         |
| `ItemInnerSpacing`    | 8, 6         |                                          |
| `ScrollbarSize`       | 14           |                                          |
| `GrabMinSize`         | 12           |                                          |
| `WindowBorderSize`    | 0            | flat fullscreen window                   |
| `ChildBorderSize`     | 1            | subtle pane separation                   |
| `FrameBorderSize`     | 0            |                                          |

### Layout metrics (fixed 1280×720)

| Metric                | Value                        |
|-----------------------|------------------------------|
| Menu bar height       | ImGui-computed (~30 px)      |
| Player bar height     | 140 px                       |
| Left pane width       | 45% of work-area width       |
| Right pane width      | remaining work-area width    |
| Transport button size | 48 × 48 px                   |
| Sprite atlas source   | 64 × 64 (scaled down to 48)  |

## Glyph & sprite inventory

Material Symbols codepoints used in labels (merged font, `TextColored`/`Text`):

| Name            | Codepoint  | Use                                                |
|-----------------|------------|----------------------------------------------------|
| music note      | `\ue405`   | player-bar track line                              |
| folder          | `\ue2c7`   | path header + directory rows (accent-tinted)       |
| audio file      | `\ueb82`   | file rows                                          |
| settings gear   | `\ue8b8`   | Settings menu label (shown as the gear in mockups) |
| fullscreen      | `\ue5d0`   | view toggle, WORKSPACE to VISUALIZATION            |
| fullscreen_exit | `\ue5d1`   | view toggle, VISUALIZATION to WORKSPACE            |

(The music-note / folder / audio-file codepoints are the ones already used in the current
`Gui.cpp`; settings / fullscreen / fullscreen_exit are standard Material Symbols codepoints
to add during implementation.)

Sprite atlas (`romfs/sprites/sprites.png`, keys parsed from `sprites.bin`):

| Key        | Size (px) | Use                                            |
|------------|-----------|------------------------------------------------|
| `play`     | 64 × 64   | play/pause button when stopped/paused          |
| `pause`    | 64 × 64   | play/pause button when playing (previously unused) |
| `stop`     | 64 × 64   | stop button                                    |
| `previous` | 64 × 64   | previous button                                |
| `next`     | 64 × 64   | next button                                    |
| `k7`       | 256 × 171 | About popup cassette logo                      |

## Time formatting

Position and duration are formatted `m:ss` (minutes, zero-padded seconds) by a small
local helper in `Gui.cpp` — e.g. `42.0 → "0:42"`, `217.0 → "3:37"`. Negative or NaN
inputs clamp to `0:00`.

## Mapping to `Gui` methods

The implementation (chunks 3b–3d) restructures `Gui`'s private draws to these regions:

| Region / concern    | Method                                             |
|---------------------|----------------------------------------------------|
| Theme application   | `applyTheme(Theme)` — dispatches to `StyleColorsDark/Light/Classic` |
| Top bar             | `drawTopBar(...)` (replaces `drawMainMenuBar`)      |
| About popup         | `drawAboutPopup()`                                  |
| Left pane path      | `drawCurrentPath(path)` (kept)                      |
| Left pane browser   | `drawFileBrowser(files, onFileClick, isWorking)` (kept, restyled) |
| Right pane tabs     | `drawTabsSection()` → Metadata + Playlist only      |
| Metadata tab        | `drawFileMetadata()` (absorbs old track-info table) |
| Playlist tab        | `drawTabPlaylist()`                                 |
| Player bar          | `drawPlayerBar(status, onButtonClick)` (absorbs `drawPlayerControls` + progress) |
| Time helper         | local `formatTime(double)` in `Gui.cpp`             |

Dropped: `drawTrackInformation`, `drawTabSettings`, `drawTabAbout` (content moved to the
Metadata tab, Settings menu, and About popup respectively).
