# TODO_3 — UI redesign: docs/ui-design.md + Gui implementation

> The presentation half of the original redesign plan; its player-side half is TODO_2. Requires TODO_1 (UiState/UiActions) and TODO_2 (PlaybackStatus in UiState) — with those in place this item is Gui-only.

## Task chunks (implement, verify, and commit one at a time)

- [ ] **3a — Design doc**: write `docs/ui-design.md` only (mockups, palette tables, metrics, glyph inventory). No code; the doc is the deliverable the user reviews.
- [ ] **3b — Theme system**: `src/gui/Theme.h` + `Gui::applyTheme(Theme)` (both palettes + shared vars), called from `initialize()` with dark default; delete the manual style block in main.cpp. No layout change yet — verify the app comes up in the new dark palette (flip the default to light once to check that palette too).
- [ ] **3c — Layout restructure**: `drawTopBar` (Settings menu with the theme radio calling `applyTheme`, About popup with k7 logo), left/right child panes, player bar child (track line, m:ss progress from `state.status`, transport with play/pause sprite swap), tabs reduced to Metadata + Playlist; remove the ImGui demo window from main.cpp. Verify against the doc.
- [ ] **3d — View mode**: `src/gui/ViewMode.h`, `m_viewMode`, top-bar toggle button, VISUALIZATION mode drawing only the top bar. Verify audio keeps playing while collapsed.

Each chunk ends with green desktop + Switch builds, a `docs/ui.md` update if classes/methods changed, user verification, then a commit.

## Context

The current UI is the stock ImGui light style with a menu bar and a two-column table (file browser left, player controls + info + tabs right). It looks dated. Decisions made:

- **Layout: bottom player bar** — browser and tabs fill the screen; a full-width persistent player bar at the bottom carries now-playing info, progress, and transport.
- **Theme: both, switchable** — custom dark and light palettes with a runtime toggle.
- **Scope: doc + implementation** — write `docs/ui-design.md` (ASCII mockups, the design source of truth), then restructure `src/gui/Gui.{h,cpp}` to match.

Window is fixed 1280×720 (Switch resolution). Fonts: Roboto 22px + Material Symbols merged. Sprite atlas: `play`, `pause`, `stop`, `previous`, `next` (64×64) and `k7` logo (256×171) — note `pause` exists but is unused today.

## Target design (goes into docs/ui-design.md, expanded)

```
┌──────────────────────────────────────────────────────────────┐
│ OSP2                                        ⚙ Settings  About │  top bar (menu bar)
├──────────────────────────────┬───────────────────────────────┤
│ 📂 /music/mods               │  Metadata │ Playlist          │
│ ──────────────────────────── ├───────────────────────────────┤
│ 📁 ..                        │  Title      Cool Song         │
│ 📁 demos                     │  Format     S3M               │
│ 🎵 cool.s3m          123 KB  │  ...                          │
│ 🎵 tune.mod           45 KB  │                               │
│         (scrollable)         │                               │
├──────────────────────────────┴───────────────────────────────┤
│ 🎵 Cool Song · cool.s3m                                       │
│ 0:42 ━━━━━●─────────────────────────────────────────── 3:37  │  player bar (~120px)
│                    ⏮    ⏯    ⏹    ⏭                         │
└──────────────────────────────────────────────────────────────┘
```

- **Top bar**: app title left; `Settings` menu (theme radio: Dark/Light) and `About` (popup with k7 logo) right — replaces the Settings/About tabs, so the right pane keeps only **Metadata** and **Playlist**.
- **Left pane** (~45% width, full height between bars): current path header + file browser table (existing clipper/columns kept, restyled: no outer borders, row hover highlight). Loading overlay behavior kept.
- **Right pane**: tab bar with Metadata / Playlist.
- **Player bar** (full width, ~120px): track line ("Title · filename" from `UiState::status`, or "No track" when stopped), display-only progress bar with `m:ss` position/duration on each side (seek is future work — plugins have no seek), transport ImageButtons centered. PLAY_PAUSE shows the `pause` sprite while playing, `play` otherwise. Buttons shrink to 48×48 to fit the bar.
- **Themes**: dark (near-black bg, one accent, rounded frames ~6px, generous spacing) and light equivalents — palette tables specified in the doc; shared metrics (rounding, padding) identical across themes. Dark is the default.
- **View modes**: `WORKSPACE` (everything above) and `VISUALIZATION` — only the top bar is drawn; panes and player bar are hidden and the whole area below is left empty (GL clear color shows through), reserved for the future visualization system. Toggled by an icon button at the far right of the top bar (Material Symbols fullscreen/collapse glyph). The doc documents the reserved region (viewport work area minus menu bar) so the visualizer can later render there — either raw GL under ImGui or a draw hook receiving that rect.

```
VISUALIZATION mode:
┌──────────────────────────────────────────────────────────────┐
│ OSP2                                    ⚙ Settings  About  ⛶ │  top bar only
├──────────────────────────────────────────────────────────────┤
│                                                              │
│              (empty — reserved for visualizations)           │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

## Files to change

1. **`docs/ui-design.md`** (new) — the full design doc: overall mockup, per-region ASCII mockups + behavior notes (states: stopped/playing/paused/loading), both palette tables with hex values, shared style metrics, icon/glyph inventory. Written first; implementation follows it.

2. **`src/gui/Theme.h`** (new) — `enum class Theme { DARK, LIGHT };`.
   **`src/gui/ViewMode.h`** (new) — `enum class ViewMode { WORKSPACE, VISUALIZATION };`.

3. **`src/gui/Gui.{h,cpp}`** — the restructure:
   - `void applyTheme(Theme)` — sets all ImGui style colors + shared vars; called from `initialize()` (dark default) and from the Settings menu. Gui keeps `m_theme` for the menu checkmark (presentation state only). Persistence of the choice comes with TODO_6.
   - `drawUserInterface(const UiState &, const UiActions &)` — signature unchanged (TODO_1); the player bar reads `state.status`.
   - Replace `table_area` 2-column table with computed child regions: left/right `BeginChild` panes above a full-width player-bar child of fixed height.
   - New/changed private draws: `drawTopBar` (menus + view-mode toggle button, replaces `drawMainMenuBar`), `drawPlayerBar(status, onButtonClick)` (absorbs `drawPlayerControls` + progress row), `drawTabsSection` reduced to Metadata + Playlist, `drawAboutPopup`; drop `drawTrackInformation` (content merges into the Metadata tab — placeholder until TODO_5 fills it), `drawTabSettings`, `drawTabAbout`.
   - View mode: Gui holds `m_viewMode` (presentation state, like `m_theme`); `drawUserInterface` draws the top bar always, and skips the fullscreen window (panes + player bar) entirely in `VISUALIZATION` mode — the area below stays empty for the future visualizer.
   - Time formatting helper (`m:ss`, static/local).

4. **`src/main.cpp`** — delete the manual `style.FramePadding/TabRounding` block (superseded by `applyTheme`); **remove the ImGui demo window**.

5. **`docs/ui.md`** — update class diagram (CLAUDE.md rule): Theme, ViewMode, new Gui methods.

No CMakeLists.txt change (all new files are headers).

## Coordination

- **Requires TODO_1 + TODO_2** (UiState/UiActions with `status` member).
- **TODO_4** fills the left pane with the real file browser (this TODO restyles the placeholder listing; TODO_4 keeps that styling).
- **TODO_5** provides the Metadata tab's real content.
- **TODO_6** persists the theme choice and moves its source of truth to settings; the Settings menu created here is where TODO_6's plugin-config UI lands.

## Order & verification

1. Write `docs/ui-design.md` — the spec.
2. Gui restructure + theme, main.cpp cleanup — build.
3. Update docs/ui.md.
4. **Verify**: desktop build (`cmake --build cmake-build-debug`) and Switch build (`cmake-build-switch`, per CLAUDE.md) both green; run `./cmake-build-debug/OSP2` from repo root — check layout matches the doc, progress bar advances on the test track, play/pause sprite swaps, theme toggle switches palettes live, view-mode toggle collapses to top-bar-only and back (audio keeps playing while collapsed), no ImGui assert (Begin/End balance), clean exit.
