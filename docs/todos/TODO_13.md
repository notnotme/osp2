# TODO_13 — Enable clang-format + lint tooling

> Two chunks. The project has no formatter or linter config of its own. Add a `.clang-format` that encodes the current house style, CMake `format` / `format-check` targets, and a `.clang-tidy` — all **scoped to `src/` only**. Do this **early** (right after TODO_12) so the whole-tree reformat lands on a settled `main.cpp` and every later feature diff is style-checked. Independent of the other items.

## Context

No OSP2-owned formatter/linter config exists anywhere: there is **no** `.clang-format`, `.clang-tidy`, `.editorconfig`, `.pre-commit-config.yaml`, or CI at the repo root or under `src/`. The only such file is `external/imgui/.editorconfig`, which belongs to the vendored submodule and must not be OSP2's concern. `CMakeLists.txt` sets C++20 (lines 4–5), lists sources in `add_executable(OSP2 …)` (lines 17–44; OSP2 sources at 28–43, vendored ImGui at 20–26) and has **no custom targets** — a `format` target is entirely new. The desktop/Switch split is `if (NINTENDO_SWITCH) … else () … endif ()`; note the Switch-only source `src/player/plugins/switch_compat.c` (line 57, a `.c` file) is added conditionally.

**Scope is `src/` only** — ~47 `.cpp`/`.h` files (16 `.cpp`, 31 `.h`) plus `switch_compat.c`. The formatter and any target must **exclude `external/` and `cmake-build-*`**, or the first run reformats thousands of ImGui lines.

**Tooling is not installed here** — `which clang-format clang-tidy` finds neither. The spec must document installing them and **pinning a version** (clang-format output differs across releases, so an unpinned tool causes churn between contributors).

**House style to encode** (observed in `Application.cpp`, `Gui.cpp`, `PlayerController.h`): 4-space indent; **namespace bodies are indented**; attached/K&R braces (`{` on the same line for functions, classes, control statements); `PointerAlignment: Right` (`T *p`, `T &r`); `m_` on data members, `SCREAMING_SNAKE_CASE` for constants/macros/enum values; `#ifndef` include guards (not `#pragma once`); include grouping own-header-first then a sorted `<...>` std block then project `"..."`; braces always used even for single-statement bodies. Two constraints matter: set **`ReflowComments: false`** so the identical GPL-3.0 header block (lines 1–18 of every file) is never mangled, and pick `MaxEmptyLinesToKeep: 2` since the tree has occasional double blank lines after include blocks (avoids needless churn). Column limit ~120, or `0` to avoid reflowing long comment lines — choose during implementation and verify the sweep stays whitespace-only.

## Task chunks (implement, verify, and commit one at a time)

- [x] **13a — `.clang-format` + one-time sweep**: author `.clang-format` at the repo root encoding the style above (start from `BasedOnStyle: LLVM` or `Google` and override to match; verify against 2–3 representative files before sweeping). Run it once over `src/` only (exclude `external/`, `cmake-build-*`). Commit the config **plus** the resulting reformat as a single mechanical, whitespace-only change. Verify: desktop + Switch both build unchanged (behaviour-preserving); `git diff -w` on the sweep commit is empty (only whitespace/formatting changed, no token changes); `external/` is untouched; the GPL headers are intact.
- [ ] **13b — CMake `format` / `format-check` targets + `.clang-tidy` (lint)**: add `add_custom_target(format …)` (rewrites in place) and `format-check` (dry-run / `--dry-run --Werror`) to `CMakeLists.txt`, each operating on a **file glob of `src/`** (a glob, not the executable's source list, because `switch_compat.c` is added conditionally). Add a root `.clang-tidy` with a conservative, low-noise check set (e.g. `bugprone-*`, `performance-*`, a curated `readability-*`) and pin the version. Document install + `cmake --build … --target format` / `format-check` in `README.md`, and update the CLAUDE.md line "There are no tests or linters configured." to reflect the new tooling. Optionally add a GitHub Actions `format-check` workflow (mention only — not required). Verify: `format-check` passes on the already-formatted tree; the new targets don't participate in / affect the normal build; the Switch build is unaffected.

Each chunk ends with green desktop + Switch builds, docs updated (`README.md`, `CLAUDE.md`), user verification, then a commit. Run cpp-reviewer on the (config/CMake) diff before committing; the 13a sweep itself is mechanical and needs a whitespace-only check rather than a full review.

## Files to change

1. **`.clang-format`** (repo root, new) — the style config.
2. **All `src/**` `.cpp`/`.h`** — reformatted by the 13a sweep (whitespace only).
3. **`.clang-tidy`** (repo root, new) — the lint check set (13b).
4. **`CMakeLists.txt`** — `format` / `format-check` custom targets (13b), after the `add_executable` block (~line 44); glob `src/` files, exclude `external/`.
5. **`README.md`** and **`CLAUDE.md`** — install + usage notes; update the "no linters configured" line.

The build's `add_executable` source list is unchanged (no new compiled file).

## Docs

- **`README.md`** / **`CLAUDE.md`** — how to install the pinned clang-format/clang-tidy and run `format` / `format-check`; note the `src/`-only scope (never `external/`).
- No `docs/*.md` class diagram applies (tooling, not a code domain).

## Coordination

- Independent, but **recommended right after TODO_12** so the sweep runs on the restructured `main.cpp` and all subsequent feature work (TODO_14–TODO_19) is written against the enforced style. Landing it later means one large reformat diff after more code has accreted.

## Verification

- Desktop + Switch builds green **after** the reformat (per CLAUDE.md) — behaviour-preserving.
- `git diff -w` on the 13a commit shows no token-level changes; `external/` and `cmake-build-*` untouched; GPL headers intact.
- `cmake --build … --target format-check` passes on the formatted tree and fails on a deliberately misformatted file; the target is decoupled from the normal build.
- `.clang-tidy` runs on `src/` without a wall of false positives (tune the check set until signal-to-noise is acceptable).
