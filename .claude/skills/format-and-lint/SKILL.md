---
name: format-and-lint
description: Format and lint OSP2 C++ with clang-format / clang-tidy (the src/-only .clang-format and .clang-tidy exported from CLion). Use after writing or changing C++ under src/, before committing, or when asked to format/lint code.
---

# Format and lint

OSP2 enforces house style with **clang-format** and **clang-tidy**, configured by `.clang-format` and `.clang-tidy` at the repo root. Both were exported from the maintainer's CLion profile and are the single source of truth — do not hand-tweak the check/style set to taste; if the style needs to change, change the config deliberately.

**Scope is `src/` only.** Never format or lint `external/` (vendored ImGui) or `cmake-build-*`.

## Tools

clang-format and clang-tidy **18** (pinned — output differs across releases):

```sh
sudo apt install clang-format-18 clang-tidy-18
```

The CMake targets locate the binary as `clang-format-18`/`clang-tidy-18` first, then plain `clang-format`/`clang-tidy`.

## Formatting

Preferred — the CMake targets (glob all of `src/`, so they include the Switch-only `switch_compat.c`):

```sh
cmake --build <build-dir> --target format        # rewrite src/ in place
cmake --build <build-dir> --target format-check   # dry-run; non-zero exit on any diff (CI / pre-commit)
```

On just the files you touched, without a build dir:

```sh
clang-format -i --style=file src/path/One.cpp src/path/Two.h
```

`--style=file` makes it read the repo `.clang-format`. `ReflowComments: false` protects the GPL-3.0 header block — never let it get mangled.

## Linting

clang-tidy needs a compile database. Configuring any desktop build dir emits `compile_commands.json` (CMake sets `CMAKE_EXPORT_COMPILE_COMMANDS`). Then:

```sh
cmake --build <build-dir> --target tidy          # lint every src/ .cpp
clang-tidy -p <build-dir> src/path/One.cpp        # lint one translation unit
```

The `tidy` target lints `.cpp` translation units (all present in the desktop compile database); headers are covered transitively via `.clang-tidy`'s `HeaderFilterRegex: src/.*`.

`.clang-tidy` mirrors CLion's default clang-tidy check set, plus `readability-identifier-naming` — which encodes the two rules clang-format cannot express: **camelBack** function/method names and the **`m_`** prefix on private/protected members. Treat naming warnings as real; fix the code, don't loosen the rule.

## When to run

- After writing or changing any C++ under `src/`: run `format` (or `clang-format -i` on the touched files).
- Before committing: `format-check` must pass; run `tidy` on changed translation units and address findings.
