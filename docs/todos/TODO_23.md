# TODO_23 — Right-align file size in the browser

> File sizes in the browser's Size column are left-aligned; right-align them so they line up on the column's right edge. Single chunk, presentation-only.

## Context

`Gui::drawFileBrowser` (`src/gui/Gui.cpp:479-631`) lists entries in an ImGui table (`BeginTable("file_browser", 3, ...)`, `:507`) with columns Name (`WidthStretch`), Type (`WidthFixed`), Size (`WidthFixed`) declared at `:526-530`. The Size cell is drawn left-aligned at `:570-573`:

```cpp
ImGui::TableNextColumn();
if (!file_entry.is_directory) {
    ImGui::TextUnformatted(formatSize(file_entry.file_size).c_str());
}
```

`formatSize` (`Gui.cpp:101-110`) returns `"N B"` / `"N.N KB"` / `"N.N MB"`.

## The fix (single chunk)

Right-align the size text within its fixed column, reusing the existing right-align idiom already used in this file for the "Track n/N" indicator (`Gui.cpp:758-763`) and the duration label (`Gui.cpp:777-783`): compute `ImGui::CalcTextSize(text).x`, then `SetCursorPosX(GetCursorPosX() + GetContentRegionAvail().x - text_width)` before drawing. The row `Selectable` uses `ImGuiSelectableFlags_SpanAllColumns` (`:536,553,562`), so it already spans the Size cell — right-aligning the text does not affect click/selection.

## Files to change

1. **`src/gui/Gui.cpp`** — Size cell in `drawFileBrowser` (`:570-573`).

## Docs

- **`docs/ui.md`** — note only if the file-browser layout description mentions column alignment.

## Verification

- Desktop + Switch builds green.
- Sizes align flush to the right of the Size column across rows; directories (no size) still render blank.
- Row selection / the currently-playing highlight are unaffected.
