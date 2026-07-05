# TODO_16 — File-browser scroll position (top on descend, restore on back)

> Single chunk. Entering a directory should start scrolled to the top; going back to the parent should restore the scroll position you left it at. Today scroll offset bleeds between directories. Independent of the other items.

## Context

The file list is an ImGui table with a **constant** id `"file_browser"` (`src/gui/Gui.cpp:394`, drawn in `Gui::drawFileBrowser`, lines 387–513). Because the table id never changes across directories, ImGui keeps one scroll offset for it — so navigating into a folder (or back out) leaves the scroll wherever it was, which reads as a bug. There is **no scroll-position state anywhere** (grep for `SetScrollY`/`GetScrollY`/`SetScrollHereY` returns nothing).

Navigation is directional and lives in the app layer, `Application::handleDirectoryClick` (`src/Application.cpp:72–75`): the synthetic `..` row → `m_fileSystem.navigateToParent()`, any other directory → `m_fileSystem.navigateToEntry(entry)`. The `..` row is pinned by the Gui (`Gui.cpp:420`), not a real `FileSystem` entry. `Gui` is presentation-only and stateless-by-design: it only fires callbacks and does not know when navigation happened; `UiState` is rebuilt every frame and holds no scroll state.

## Task chunks (implement, verify, and commit one at a time)

- [ ] **16a — Scroll stack keyed to nav depth**: maintain a **stack of saved `GetScrollY()` values** — push the current browser scroll before descending, pop + restore it on `..`, and set scroll to `0` when a *new* directory is entered. Chosen owner: **the app layer emits a per-frame navigation signal** (direction: descend / ascend / none) into `UiState`, and `Gui::drawFileBrowser` acts on it — call `ImGui::SetScrollY(0)` on descend, `ImGui::SetScrollY(saved)` on ascend, capturing `GetScrollY()` **inside** the `BeginTable`/`EndTable` scope before the callback fires. This keeps the Gui presentation-only and the navigation truth in `Application`/`FileSystem`. State added: a `std::vector<float>` scroll stack (owner: whoever emits the signal, or the Gui reacting to it — spec the exact split during implementation) + a one-frame "navigated + direction" flag on `UiState`. Verify: scroll a long folder halfway, enter a subfolder → it starts at the top; press `..` → the parent is scrolled back to where you left it; nesting several levels deep and climbing back restores each level correctly.

This chunk ends with green desktop + Switch builds, docs updated (`docs/ui.md`, `docs/filesystem.md` if the signal originates there), user verification, then a commit. Run cpp-reviewer on the diff before committing.

## Files to change

1. **`src/gui/UiState.h`** — a one-frame navigation signal (e.g. `enum class NavKind { None, Descend, Ascend }` + the value) so the Gui knows a navigation just happened and in which direction.
2. **`src/Application.{h,cpp}`** and/or **`src/filesystem/FileSystem.{h,cpp}`** — emit that signal when `navigateToEntry` / `navigateToParent` runs, surfaced through `makeUiState`.
3. **`src/gui/Gui.{h,cpp}`** — own the `std::vector<float>` scroll stack; in `drawFileBrowser`, capture/restore scroll based on the signal (all `Get/SetScrollY` calls inside the `file_browser` table scope).

No CMakeLists.txt change.

## Docs

- **`docs/ui.md`** — document the scroll-restore behaviour and the scroll stack in the Gui.
- **`docs/filesystem.md`** — if the navigation signal originates in `FileSystem`/`Application`, note the descend/ascend indicator.

## Coordination

- Independent. Touches only the browser rendering and the navigation path.

## Verification

- Desktop + Switch builds green (per CLAUDE.md).
- Enter a directory with more entries than fit: it opens scrolled to the top.
- Scroll the parent halfway, descend, then `..` back: parent scroll is exactly restored.
- Multi-level: descend 3 folders (each opens at top), climb back with `..` three times — each parent restores its own remembered position; no bleed, no jump.
