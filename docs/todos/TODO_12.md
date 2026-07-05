# TODO_12 — main.cpp cleanup + shared `Paths.h` (BASE_PATH dedup)

> Behaviour-preserving refactor, done early so the later feature items (TODO_14–TODO_17, which all touch `main.cpp`) land on a cleaner base and a single source of path truth. Any observable behaviour change is a bug, not an improvement.

## Context

`src/main.cpp` is 353 lines and has accumulated the usual platform-entrypoint cruft:

- **`BASE_PATH` macro is defined three times** — `src/main.cpp:54`, `src/Application.cpp:25`, `src/player/plugins/SidPlugin.cpp:36` — each with the identical `#if defined(__SWITCH__)` (`"romfs:/"`) `#else` (`"romfs/"`) block. This is the duplicate the user flagged.
- **`configPath()` (`main.cpp:72–84`) and `cachePath()` (`main.cpp:88–100`) duplicate** the same `__SWITCH__` vs `SDL_GetBasePath()` writable-root boilerplate.
- **`initialize()` (`main.cpp:102–238`) is a ~136-line kitchen sink** mixing SDL/IMG init, GL attributes, window+context, controller open, ImGui init, font loading, `visualizer.create()`, settings load + theme, `player.create()`, the plugin-settings push loop (174–182), start-folder resolution (186–196), curl/network init (198–206), and the data-source construction + `[source.NAME]` INI parsing loop (208–234).
- **Scattered magic constants** — window size `1280, 720` at `:118` and again in `CursorEmulator(1280, 720)` at `:296`; font size `22.0f` twice (157–158); GL version `4/3` (113–114) vs shader `"#version 330 core"` (140); glyph range `{ 0x0030, 0xFFCB, 0 }` (156); `SwapInterval(1)` (132); clear color at `:324`.
- **`SDL_QUIT` case falls through** into `default` (`main.cpp:315`, missing `break`) — currently harmless but fragile.
- **A comment-documented scope block (292–348)** exists only to sequence `CursorEmulator` destruction before `finalize()`.

`Gui::initialize(const std::string &basePath)` already receives the asset root as a parameter rather than the macro, so the read-only root is handled two ways today (macro vs passed string) — the shared header should be the one truth both use.

## Task chunks (implement, verify, and commit one at a time)

- [ ] **12a — Shared `Paths.h`**: new header-only `src/Paths.h` (GPL header) exposing the asset root once (`BASE_PATH` macro or a `constexpr` helper) and a single writable-base helper backing `configPath()`/`cachePath()` (keeping the `__SWITCH__` → `sdmc:/switch/…` vs `SDL_GetBasePath()` split in one place). Remove the duplicated `BASE_PATH` macro from `main.cpp`, `Application.cpp`, `SidPlugin.cpp` and include `Paths.h` instead; collapse `configPath()`/`cachePath()` onto the shared helper. Header-only → no CMake change; add to `add_executable` only if a `.cpp` is introduced. Verify: desktop + Switch build; at runtime fonts still load, `osp2.ini` lands in the same spot, cache path unchanged, SID C64 ROM paths (`SidPlugin.cpp:116–118`) still resolve.
- [ ] **12b — Split `initialize()` + hoist constants**: extract cohesive helpers (e.g. `initSDLandGL()`, `initImGui()`/`loadFonts()`, `initPlayerAndSettings()`, `buildDataSources()`) out of the 136-line `initialize()`; lift the scattered magic numbers (window size, font size, GL version, deadzone/glyph range, clear color) to named `constexpr`s shared by their two use sites; fix the `SDL_QUIT` fall-through. Strictly behaviour-preserving. Verify: identical startup + shutdown behaviour on desktop and Switch (window opens, fonts render, a track plays, data sources build, clean exit); no functional diff.

Each chunk ends with green desktop + Switch builds, docs updated (`docs/application.md`), user verification, then a commit. Run cpp-reviewer on the diff before committing.

## Files to change

1. **`src/Paths.h`** (new, header-only) — single definition of the read-only asset root + writable-base helper.
2. **`src/main.cpp`** — include `Paths.h`; drop local `BASE_PATH`; collapse `configPath()`/`cachePath()`; split `initialize()`; hoist constants; fix `SDL_QUIT` fall-through.
3. **`src/Application.cpp`** — drop local `BASE_PATH`, include `Paths.h`.
4. **`src/player/plugins/SidPlugin.cpp`** — drop local `BASE_PATH`, include `Paths.h`.
5. **CMakeLists.txt** — only if `Paths.h` gains a `.cpp` (prefer header-only → no change).

## Docs

- **`docs/application.md`** — note the new `src/Paths.h` as the single source of path truth, and the split `initialize()` helper structure (update the lifecycle classDiagram/notes as touched).

## Coordination

- **Do early.** TODO_14 (visualizer restore), TODO_15 (quit button), and TODO_17 (playback overlay/errors) all edit `main.cpp` and benefit from the cleaner init and shared paths. No functional dependency the other way.

## Verification

- Desktop (`cmake --build cmake-build-debug`) + Switch (`cmake-build-switch`, per CLAUDE.md) build green after each chunk.
- **12a**: run from repo root — fonts load (proves `BASE_PATH`), `osp2.ini` read/written in the same location (proves the writable helper), a `.sid` plays (proves `SidPlugin` ROM paths). On Switch confirm `romfs:/` + `sdmc:/switch/osp2.ini` unchanged.
- **12b**: full run-through shows no behavioural difference — startup, playback, data-source construction, and clean shutdown identical to before the refactor.
