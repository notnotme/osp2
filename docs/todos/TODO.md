# TODO

Ordered backlog — items build on each other in this sequence (TODO_1 is the foundation every later item hangs member additions off; TODO_6 depends on nearly everything before it).

Large items are broken into **task chunks** (see the "Task chunks" section in each file). A chunk is the unit of work: it builds green on desktop + Switch, updates docs/, gets verified by the user, and is committed — never batch several chunks into one commit.

- [x] [TODO_1 — Separate UI actions from presentation (UiState/UiActions + Application layer)](TODO_1.md) — single chunk
- [x] [TODO_2 — Playback status API (PlaybackStatus + position/duration)](TODO_2.md) — single chunk
- [x] [TODO_3 — UI redesign: docs/ui-design.md + Gui implementation](TODO_3.md)
  - [x] 3a — Design doc (`docs/ui-design.md`)
  - [x] 3b — Theme system (`Theme.h`, `applyTheme`)
  - [x] 3c — Layout restructure (top bar, panes, player bar, tabs)
  - [x] 3d — View mode (WORKSPACE / VISUALIZATION)
- [x] [TODO_4 — Real threaded FileBrowser (DataSource abstraction + local source)](TODO_4.md)
  - [x] 4a — Threaded FileSystem backend + wiring (DataSource, LocalDataSource, requestFile)
  - [x] 4b — Directory navigation (onDirectoryClick, "..", virtual root)
  - [x] 4c — Browser polish (Type column, sizes, icons, spinner)
- [x] [TODO_5 — Typed per-plugin track metadata (std::variant) driving the Metadata tab](TODO_5.md)
  - [x] 5a — Metadata backend (capture-at-open, fetch-on-change)
  - [x] 5b — Metadata tab UI (visit dispatch)
- [x] [TODO_6 — Plugin configuration + persisted user settings (INI)](TODO_6.md)
  - [x] 6a — Settings domain + user settings (theme, default_folder)
  - [x] 6b — Plugin settings backend (PluginSetting, OpenMpt tunables)
  - [x] 6c — Settings UI (generic widget loop)
- [x] [TODO_7 — Remote data source: Modland FTP via libcurl](TODO_7.md)
  - [x] 7a — curl dependency + FTP listing (MLSD)
  - [x] 7b — Download-to-cache + playback
  - [x] 7c — Robustness + polish (overlay label, cancel button, sanitization, timeouts; Switch hardware verified)
  - [x] 7d — User-defined FTP sources (optional, requires TODO_6)
- [x] [TODO_8 — Visualization plugin system (VisualizerPlugin + Bars visual)](TODO_8.md) — requires TODO_3 (view mode); chunk 8a is backend-only and independent
  - [x] 8a — Audio tap (AudioTap seqlock + PlayerController::readLatestAudio)
  - [x] 8b — Visualizer skeleton + wiring (VisualizerPlugin, VisualizerController, onRenderVisualization hook, DebugVisualizer)
  - [x] 8c — BarsVisualizer (vertical bars from tapped audio)
  - [x] 8d — GL shader-quad plugin + selector (future / optional)
- [x] [TODO_9 — Additional decoder plugins (GmePlugin / SidPlugin / Sc68Plugin)](TODO_9.md) — requires TODO_5 + TODO_6; 9c/9e gated on maintainer-built Switch portlibs
  - [x] 9a — GmePlugin (libgme; NSF/SPC/VGM/GBS/…, both platforms today) + int16 audio pipeline + GME settings
  - [x] 9c — SidPlugin (libsidplayfp; needs switch-libsidplayfp portlib — P1)
  - [x] 9e — Sc68Plugin (libsc68; needs sc68 portlib desktop + Switch — P2)
- [x] [TODO_10 — Switch controller → mouse/scroll emulation (CursorEmulator)](TODO_10.md) — greenfield; no decoder dependency
  - [x] 10a — Controller cursor + clicks (left stick move, A/X click)
  - [x] 10b — Scroll + speed modifiers + polish (right stick scroll, L/R slow/fast)
- [x] [TODO_11 — Back-fill THIRD_PARTY_NOTICES.md](TODO_11.md) — priority; single chunk; docs-only
- [x] [TODO_12 — main.cpp cleanup + shared Paths.h (BASE_PATH dedup)](TODO_12.md) — behaviour-preserving; do early
  - [x] 12a — Shared Paths.h (dedup BASE_PATH + config/cache helpers)
  - [x] 12b — Extract a Platform class; eliminate globals
- [x] [TODO_13 — Enable clang-format + lint tooling](TODO_13.md) — do early (after TODO_12); scoped to src/
  - [x] 13a — .clang-format + one-time sweep
  - [x] 13b — CMake format/format-check targets + .clang-tidy
- [x] [TODO_14 — Persist selected visualizer](TODO_14.md) — requires TODO_6 + TODO_8; single chunk
- [x] [TODO_15 — Quit-app button](TODO_15.md) — single chunk
- [x] [TODO_16 — File-browser scroll position (top on descend, restore on back)](TODO_16.md) — single chunk
- [x] [TODO_17 — Playback loading overlay + error notifications](TODO_17.md)
  - [x] 17a — Async decode + loading overlay
  - [x] 17b — Playback error notifications (popup)
- [x] [TODO_18 — Subtrack navigation (NEXT/PREVIOUS step subtracks)](TODO_18.md) — requires TODO_9; GME multi-track
  - [x] 18a — Plugin subtrack API + GME impl + controller pass-through
  - [x] 18b — Wire NEXT/PREVIOUS + auto-advance to subtracks
- [x] [TODO_19 — Loop-song setting](TODO_19.md) — requires TODO_6; single chunk
- [x] [TODO_20 — Non-UTF-8 track metadata (charset transcoding + CJK font)](TODO_20.md) — requires TODO_9 + TODO_5; independent
  - [x] 20a — Charset transcoding to UTF-8 at the plugin boundary
  - [x] 20b — CJK-capable font
- [x] [TODO_21 — Accurate NES/NSF duration (drop the fake 2:30)](TODO_21.md) — requires TODO_9 + TODO_18; single chunk
- [x] [TODO_22 — No UI blink when next/prev crosses a file boundary](TODO_22.md) — requires TODO_17 + TODO_18; single chunk
- [x] [TODO_23 — Right-align file size in the browser](TODO_23.md) — single chunk; presentation-only
- [x] [TODO_24 — Better About box (version, author, credits, link)](TODO_24.md) — single chunk
- [x] [TODO_25 — Hide ".." when browsing the root (sources)](TODO_25.md) — single chunk
- [x] [TODO_26 — Remove the default bundled sound (romfs/music/)](TODO_26.md) — single chunk
- [x] [TODO_27 — Reorder Settings menu → Theme, Visualizer, Plugins](TODO_27.md) — single chunk; presentation-only
- [x] [TODO_28 — Playlist tab (add/remove/shuffle/repeat)](TODO_28.md) — requires TODO_9 + TODO_17 + TODO_18; multi-chunk
  - [x] 28a — PlayList module + wiring
  - [x] 28b — Draw the Playlist tab (tofu state icons)
  - [x] 28c — Add to playlist (right-click context menu)
  - [x] 28d — Remove from playlist
  - [x] 28e — Play + Shuffle + Repeat
- [x] [TODO_29 — GME durations from a companion `.m3u` (local)](TODO_29.md) — requires TODO_21; GmePlugin combined + exploded m3u overlay
- [x] [TODO_30 — SID durations from the HVSC Songlengths database](TODO_30.md) — requires TODO_9; bundled Songlengths.md5 (load-if-present)

Batch from the 2026-07 whole-codebase architecture audit (cpp-architect, findings verified) — TODO_31 and TODO_34 are independent; TODO_33 requires TODO_32:

- [x] [TODO_31 — Filesystem robustness: cancellable playlist-replay fetches + FTP listing dedup](TODO_31.md) — independent; top value (user-facing Cancel defect)
  - [x] 31a — Work-source cancel fix (`FileSystem::m_workSource`)
  - [x] 31b — FTP listing-splitter dedup (`parseListing`)
- [ ] [TODO_32 — UI seam hardening (visualizer into Application, designated initializers, Gui helpers take bundles, SettingsKeys)](TODO_32.md) — behavior-preserving; blocks TODO_33
  - [x] 32a — Visualizer bridge into Application + designated initializers
  - [x] 32b — Gui draw helpers take `(state, actions)` + `Gui final` + cleanups
  - [ ] 32c — SettingsKeys.h (single-source INI section/key names)
- [ ] [TODO_33 — Playlist "now playing" highlight by index](TODO_33.md) — requires TODO_32; single chunk; fixes false highlights
- [ ] [TODO_34 — Player plugins: RAII lifecycle + PluginUtil dedup](TODO_34.md) — independent; most churn, do last
  - [ ] 34a — RAII collapse (`PlayerPlugin` ctor/dtor; NOT `VisualizerPlugin`; also empty `PlayList` lifecycle)
  - [ ] 34b — PluginUtil.h dedup + `statusLocked()` + OpenMpt title cache

- [ ] [TODO_35 — No overlay blink between "Downloading..." and "Loading..." (fetch→decode hand-off)](TODO_35.md) — independent; single chunk; found on Switch hardware during TODO_31 verification
