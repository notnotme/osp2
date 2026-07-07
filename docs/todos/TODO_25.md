# TODO_25 — Hide ".." when browsing the root (sources list)

> At the virtual root (the list of data sources) the browser still shows a ".." row that does nothing useful. Hide ".." when at the root; keep it inside a source. Single chunk.

## Context

`Gui::drawFileBrowser` (`src/gui/Gui.cpp:479-631`) draws a ".." row **unconditionally** at `:532-539`:

```cpp
ImGui::TableNextRow();
ImGui::TableNextColumn();
ImGui::TextColored(folder_color, "");
ImGui::SameLine();
if (ImGui::Selectable("..", false, ImGuiSelectableFlags_SpanAllColumns)) {
    onDirectoryClick(FileEntry{"..", 0, "Folder", true});
}
```

".." is synthetic; clicking it calls `Application::handleDirectoryClick` (`Application.cpp:75`) → `m_fileSystem.navigateToParent()`, which already early-returns at the virtual root (`FileSystem.cpp:125-126`). So at the root ".." is a no-op that only adds clutter.

"At root" = the FileSystem virtual root = `FileSystem::getPath().empty()` (`FileSystem.h:108`; internally `m_activeSource == nullptr`). `Application::makeUiState` (`Application.cpp:260,277`) already turns the empty path into the display string `"Sources"`, but `UiState` (`src/gui/UiState.h`) carries **no explicit root flag**.

## The fix (single chunk)

- Add a bool (e.g. `isAtRoot`) to `UiState` (`src/gui/UiState.h`), set in `makeUiState` from `m_fileSystem.getPath().empty()` (`Application.cpp:259-286`).
- Thread it to `drawFileBrowser` (via `drawUserInterface`, call at `Gui.cpp:906-914`) and guard the ".." row (`:532-539`) on `!isAtRoot`.
- Prefer the bool over string-comparing the display path against `"Sources"` (fragile).

## Files to change

1. **`src/gui/UiState.h`** — add `isAtRoot`.
2. **`src/Application.cpp`** — set it in `makeUiState`.
3. **`src/gui/Gui.cpp`** — thread the flag and guard the ".." row.

## Docs

- **`docs/ui.md`** / **`docs/filesystem.md`** — note that ".." is suppressed at the virtual root.

## Verification

- Desktop + Switch builds green.
- At the sources list: no ".." row.
- Inside a source: ".." reappears and still navigates to the parent (and to the root from a source's top level).
