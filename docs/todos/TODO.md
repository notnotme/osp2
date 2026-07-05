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
- [ ] [TODO_6 — Plugin configuration + persisted user settings (INI)](TODO_6.md)
  - [x] 6a — Settings domain + user settings (theme, default_folder)
  - [x] 6b — Plugin settings backend (PluginSetting, OpenMpt tunables)
  - [ ] 6c — Settings UI (generic widget loop)
- [ ] [TODO_7 — Remote data source: Modland FTP via libcurl](TODO_7.md)
  - [ ] 7a — curl dependency + FTP listing (MLSD)
  - [ ] 7b — Download-to-cache + playback
  - [ ] 7c — Robustness + polish (overlay label, failures, Switch hardware)
  - [ ] 7d — User-defined FTP sources (optional, requires TODO_6)
- [ ] [TODO_8 — Visualization plugin system (VisualizerPlugin + Bars visual)](TODO_8.md) — requires TODO_3 (view mode); chunk 8a is backend-only and independent
  - [ ] 8a — Audio tap (AudioTap seqlock + PlayerController::readLatestAudio)
  - [ ] 8b — Visualizer skeleton + wiring (VisualizerPlugin, VisualizerController, onRenderVisualization hook, DebugVisualizer)
  - [ ] 8c — BarsVisualizer (vertical bars from tapped audio)
  - [ ] 8d — GL shader-quad plugin + selector (future / optional)
