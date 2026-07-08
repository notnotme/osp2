# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

OSP2 is a chiptune player written in C++20, built with SDL2 + OpenGL + Dear ImGui. It targets both desktop Linux and Nintendo Switch (homebrew, via devkitPro).

## Build

First checkout: `git submodule update --init` (Dear ImGui lives in `external/imgui`).

Desktop (requires, installed system-wide: SDL2, SDL2_image, OpenGL, glad, the decoder libraries libopenmpt, libgme, libsidplayfp, and sc68 (libsc68 + file68 + unice68), and libcurl):

```sh
cmake -B cmake-build-debug
cmake --build cmake-build-debug
```

Run the binary from the repository root — asset paths are relative (`romfs/`):

```sh
./cmake-build-debug/OSP2
```

Nintendo Switch builds use the devkitPro CMake toolchain, which defines `NINTENDO_SWITCH`; that branch in CMakeLists.txt switches glad to pkg-config, adds ImGui compile definitions, and packages a `.nro` with `romfs/` embedded (accessed as `romfs:/` via the `__SWITCH__` branch in `assetPath()`, src/Paths.h).

**Every change must compile for the Switch too.** devkitPro is installed at `/opt/devkitpro`. Verify in `cmake-build-switch`:

```sh
mkdir -p cmake-build-switch && cd cmake-build-switch
source /opt/devkitpro/switchvars.sh
cmake -G"Unix Makefiles" -DCMAKE_C_FLAGS="$CFLAGS $CPPFLAGS" -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake ..
make -j16
```

Keep desktop and Switch paths working when touching CMakeLists.txt or platform-dependent code. Decoder libraries must be available as devkitPro portlibs (pkg-config), not only vcpkg/apt — prefer `pkg_check_modules` over `find_package` for them.

The `run_program_on_switch` skill deploys the `.nro` to real hardware over the network — **always ask the user before running it**; the Switch must be on the netloader screen and the user decides when to test on hardware.

There are no tests configured. New source files must be added manually to the `add_executable` list in CMakeLists.txt. Formatting and static analysis are configured via `.clang-format` and `.clang-tidy` (scoped to `src/`, exported from CLion) — see the `format-and-lint` skill for the tools and `format` / `format-check` / `tidy` targets. The cpp-expert agent formats its output; the cpp-reviewer agent runs clang-tidy.

## License headers

OSP2 is GPL-3.0-or-later (see `LICENSE`). Every `.cpp`/`.h` under `src/` must start with this header — include it in every new file you create (files in `external/` are third-party and must not be touched):

```cpp
/*
 * Copyright (C) 2026 Romain Graillot
 *
 * This file is part of OSP2.
 *
 * OSP2 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OSP2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
```

## Agents (.claude/agents/)

Use the project subagents for C++ work:

- **cpp-architect** — design-pattern analysis. Use it to audit the codebase (or one domain/class) for places where a GoF design pattern (refactoring.guru catalog) would make the current implementation clearer, simpler, or more robust; it reports refactoring work orders to hand to cpp-expert without modifying code.
- **cpp-expert** — writing and refactoring code. Delegate non-trivial C++20 implementation work (new classes, Dear ImGui UI, refactors, layering/DDD decisions) to it with a concrete task and the relevant file paths.
- **cpp-reviewer** — reviewing code. Before presenting a finished task (or task chunk) for user verification, have it review the diff; it reports correctness bugs, clean-code violations, and layering breaches without modifying code. Address real findings before asking the user to verify.

Trivial mechanical edits (typo fixes, adding a file to CMakeLists.txt, doc updates) don't need either agent.

## Backlog (docs/todos/)

Planned work lives in `docs/todos/`: `TODO.md` is the ordered index; each `TODO_N.md` is a full spec. Items are sequenced — respect the dependency order stated in each file. Large items carry a **"Task chunks"** section: the chunk is the unit of work. Implement exactly one chunk at a time — build (desktop + Switch), update docs/, have the user verify, commit, tick the chunk's checkbox in both the TODO file and `TODO.md` — then move to the next. Never batch multiple chunks into one commit or start a chunk while the previous one is unverified.

## Branches & commits

Never work directly on `master`. Before starting a task (e.g. a TODO item), create a new branch from `master` named per [Conventional Branch](https://conventionalbranch.org/) (`feature/...`, `bugfix/...`, ...); all of the task's chunks are committed on that branch. Merge back only when the whole task is done and verified.

Use [Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/) (`feat:`, `fix:`, `refactor:`, `docs:`, `build:`, ...) — the `conventional-commit` and `conventional-branch` skills have the full rules. Commit after each task is finished — meaning it builds and compiles (desktop + Switch), docs/ is updated, and the user has verified it. Don't commit unverified work.

When a task (or task chunk) is finished, announce it by saying exactly: **"All your base are belong to us"**.

## Documentation (docs/)

Maintain `docs/` — one markdown file per domain (`audio.md`, `ui.md`, `filesystem.md`, ...) containing a Mermaid `classDiagram` of that domain plus short notes on non-obvious behavior (threading contracts, formats). When you add, remove, or restructure classes, update the matching domain file in the same change; create a new domain file when a new domain appears. Update docs and diagrams after each task is done, **before** asking the user to verify — docs are part of the task, not a follow-up.

## Architecture

The codebase is intentionally small, with a strict separation between platform/lifecycle, an application/orchestration layer, presentation, and data:

- **src/Platform.cpp** — owns everything platform-related: SDL/OpenGL/ImGui init and shutdown, font loading, the event loop, and the per-frame render loop. It also owns the subsystems (`Gui`, `PlayerController`, `FileSystem`, `VisualizerController`, `Settings`, `Application`) as value members — there are no globals — and exposes `create()`/`run()`/`destroy()`. Each frame it calls `app.update()`, builds a `UiState` snapshot via `app.makeUiState()`, and hands it to `gui.drawUserInterface(state, actions)`. **src/main.cpp** is just the entry point: it constructs a `Platform` and calls the three lifecycle methods. See docs/platform.md.
- **src/Application.cpp** — the orchestration layer between presentation and data. It wires `Gui`, `PlayerController`, `FileSystem`, `Settings`, and `VisualizerController` together, exposes `makeUiState()` (an immutable per-frame view snapshot) and `makeUiActions()` (the callback bundle the Gui fires), and routes UI actions — button clicks, directory/file navigation, theme/visualizer/plugin-setting changes — to the right subsystem.
- **src/gui/** — `Gui` is presentation-only. `drawUserInterface(const UiState &state, const UiActions &actions)` takes an immutable per-frame `UiState` snapshot (src/gui/UiState.h) plus a `UiActions` bundle of callbacks (src/gui/UiActions.h), including `onButtonClick` keyed by `ButtonId` (src/gui/ButtonId.h). The Gui holds only presentation state — sprite atlas texture, current theme, view mode, and popup/settings-edit latches — never application/domain state.
- **src/filesystem/** — `FileSystem` is a threaded directory browser over a set of `DataSource`s: local storage (`LocalDataSource`) and the remote Modland archive over FTP (`FtpDataSource`, libcurl). Directory scans run on a worker thread; the main thread reads finished listings (`FileEntry`: name, size, is_directory) via `update()`, and the `Application` feeds them to the Gui. See docs/filesystem.md for the threading contract.
- **src/player/** — playback core. `PlayerController` owns the SDL audio device (48 kHz signed-16-bit stereo, `AUDIO_S16SYS`, pull-model callback; plugins render native int16) and a vector of `PlayerPlugin` implementations (one per decoder library; `plugins/OpenMptPlugin` wraps libopenmpt). Plugin selection is by file extension. Threading contract: the SDL audio thread calls `decode()` under `m_mutex`, which also guards `m_state`/`m_activePlugin`; end-of-track is signaled via an atomic flag polled by the main loop (`consumeTrackEnded()` drives auto-advance). Track teardown never happens on the audio thread; `destroy()` closes the device before destroying plugins. Adding a decoder = one plugin class + one `emplace_back` in `PlayerController::create()` + one CMake stanza (e.g. libgme, libsidplayfp, libsc68, all shipped).
- **src/settings/, src/visualizer/, src/input/** — supporting domains: `Settings` (INI-backed user and per-plugin config), `VisualizerController` + visualizer plugins (real-time audio visualizations fed by the audio tap), and `CursorEmulator` (Switch controller → on-screen cursor/scroll). Each has a matching docs/ domain file.
- **external/imgui/** — Dear ImGui as a pristine git submodule (pinned to v1.92.8; `git submodule update --init` after cloning). Never edit it. The Switch glad integration lives in `src/gui/imgui_impl_opengl3_glad.cpp`, a wrapper that includes glad before the upstream OpenGL3 backend and is what CMakeLists.txt compiles instead of the backend directly.

## Assets (romfs/)

- `romfs/sprites/sprites.bin` + `sprites.png` are the sprite atlas consumed at runtime. `sprites.bin` is a custom binary format parsed in `Gui::initialize()`: `"SPSH"` signature, an int32 sprite count, then per sprite a 32-byte name followed by a packed `Sprite` struct (UV coords s/t/p/q as floats, width/height as int16).
- `romfs/raw/` holds the source images and `sprites.json` (atlas coordinates) used to produce the atlas; regenerate with the `make-spritesheet` skill after changing them.
- Icon glyphs in UI strings (e.g. `""`) come from the Material Symbols font merged into the default font in main.cpp.
