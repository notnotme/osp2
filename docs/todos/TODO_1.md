# TODO_1 — Separate UI actions from presentation (UiState/UiActions + Application layer)

> Foundation item — do this first. Every later TODO adds per-frame data or callbacks to the UI; with this layer in place, each addition is a struct member instead of a `drawUserInterface` signature change.

## Context

The callback system (`onButtonClick`/`onFileClick`) stays — it's the right shape ("UI reports intent, app decides") — but the call site in `src/main.cpp` has become a gigantic function call: six arguments, two of which are 20-line inline lambdas holding all the action logic. This refactor separates actions from the UI while keeping the callback style:

- **Callback struct** — callbacks bundled in a `UiActions` struct (built once at startup), per-frame data in a `UiState` view-model struct; the call becomes `drawUserInterface(state, actions)`.
- **Application class** — new `src/Application.{h,cpp}` use-case layer owning the ButtonId switch, `playAdjacentTrack`, the file-click check, and the end-of-track poll; main.cpp shrinks to pure platform lifecycle (SDL/GL/ImGui init, event loop).

## New files

- **`src/gui/UiState.h`** — per-frame view model (value object, never stored):
  ```cpp
  struct UiState {
      std::string currentFile;
      std::string path;
      const std::vector<FileEntry> &files;   // non-owning view, valid for the frame
      bool isWorking;
  };
  ```
- **`src/gui/UiActions.h`** — the callback bundle, wired once at init:
  ```cpp
  struct UiActions {
      std::function<void(ButtonId)> onButtonClick;
      std::function<void(const FileEntry &)> onFileClick;
      // grows with later TODOs: onDirectoryClick (TODO_4), onThemeChange / onPluginSettingChange (TODO_6)
  };
  ```
- **`src/Application.{h,cpp}`** — use-case layer, `final`, non-copyable, explicit ctor taking `PlayerController &` and `FileSystem &`:
  - `void handleButtonClick(ButtonId)` — the switch currently inline in main.cpp (incl. the `TODO(temporary)` hardcoded test track on PLAY-when-STOPPED).
  - `void handleFileClick(const FileEntry &)` — the `isSupported` + `play` check.
  - `void playAdjacentTrack(int direction)` — moves verbatim from main.cpp.
  - `void update()` — the per-frame `consumeTrackEnded()` → `playAdjacentTrack(+1)` poll.
  - `[[nodiscard]] UiState makeUiState() const` — builds the frame's view model from player + filesystem (edge translation: domain → view model lives here, not in Gui).
  - `[[nodiscard]] UiActions makeUiActions()` — returns lambdas binding `this` (called once; Application outlives the actions).

## Changed files

- **`src/gui/Gui.{h,cpp}`** — `drawUserInterface(const UiState &state, const UiActions &actions)`; private draw helpers keep their current focused parameters (`drawFileBrowser(state.files, actions.onFileClick, state.isWorking)` etc.). Include `UiState.h`/`UiActions.h` from `Gui.h`.
- **`src/main.cpp`** — add global `Application app(player, file_system);` (after `player`/`file_system` — same-TU init order is well-defined); `const auto actions = app.makeUiActions();` once after init; frame loop becomes `app.update();` + `gui.drawUserInterface(app.makeUiState(), actions);`. Both 20-line lambdas and `playAdjacentTrack` disappear from main.cpp.
- **`CMakeLists.txt`** — add `src/Application.cpp` to `add_executable`.
- **`docs/application.md`** (new domain doc, per CLAUDE.md rule) — classDiagram: Application → PlayerController + FileSystem, produces UiState/UiActions consumed by Gui.
- **`docs/ui.md`** — add UiState/UiActions value objects and the new `drawUserInterface` signature.

## Coordination (what later TODOs hang off this)

- **TODO_2**: replaces `UiState::currentFile` with a `PlaybackStatus` member.
- **TODO_3**: UI redesign keeps the `(state, actions)` signature untouched.
- **TODO_4**: adds `UiActions::onDirectoryClick`; navigation logic goes into Application.
- **TODO_5**: adds `UiState::metadata`; fetch-on-track-change logic goes into Application.
- **TODO_6**: adds `UiActions::onThemeChange` / `onPluginSettingChange`; settings change-flow goes into Application.

## Verification

- Desktop build (`cmake --build cmake-build-debug`) and Switch build (`cmake-build-switch`, per CLAUDE.md) both green.
- Run `./cmake-build-debug/OSP2` from repo root: behavior identical to before — play/pause/stop/next/previous buttons work, clicking a supported file plays it, auto-advance at end of track, clean exit.
