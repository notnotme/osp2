# TODO_4 — Real threaded FileBrowser

> Requires TODO_1 (the new callback becomes a `UiActions` member and navigation logic lives in Application). Lands after TODO_3 so the browser fills the redesigned left pane; keeps TODO_3's styling. Designed around the `DataSource` interface so TODO_7 can add remote sources (FTP) without reworking FileSystem, Gui, or Application.

## Task chunks (implement, verify, and commit one at a time)

- [x] **4a — Threaded FileSystem backend**: new `FileEntry` shape, `DataSource` interface + `LocalDataSource`, `FileSystem` create/destroy/navigateToEntry/navigateToParent/update/requestFile/consumeFetchResult with the worker thread and threading contract, main.cpp wiring (sources vector, start path, `isPlayable` predicate, destroy order) + `Application::update()` calling `file_system.update()` and routing all playback through requestFile/consumeFetchResult. Gui touched only enough to compile against the new `FileEntry` (name + size). CMakeLists gains `src/filesystem/LocalDataSource.cpp`. Verify: repo listing shows real folders + playable files.
- [x] **4b — Directory navigation**: `UiActions::onDirectoryClick`, `Application::handleDirectoryClick`, ".." pinned by the Gui, virtual root: ".." at the source root shows the sources list ("Local files"); entering it scans `/` (desktop) / `sdmc:/` (Switch). Verify: enter `romfs/music`, play a track, climb to `/`, one more ".." shows the sources list, re-enter Local files.
- [x] **4c — Browser polish**: Type column + B/KB/MB size formatting, icons driven by real data, spinner overlay (`| / ─ \`) + `BeginDisabled` while scanning (the overlay is reused by TODO_7 during downloads). Verify with a temporary `sleep_for` in the worker (remove before finishing).

Each chunk ends with green desktop + Switch builds, docs updated (`docs/filesystem.md`, `docs/ui.md`, `docs/application.md` as touched), user verification, then a commit.

## Context

`FileSystem` (src/filesystem/) currently returns ~29 hardcoded placeholder entries and `isWorking()` is always false. This item replaces it with a real, threaded directory browser:

- **Source-based browsing** — `DataSource` (domain interface) with `LocalDataSource` as the only TODO_4 implementation; a synthetic "virtual root" lists sources as folder entries. TODO_7 adds `FtpDataSource` (Modland).
- **Threaded scan** — directory listing runs on a worker thread; the UI shows a blocking overlay while it runs.
- **Loading animation: ASCII spinner** — rotating `| / ─ \` next to "Scanning...", stepped ~8×/s from `ImGui::GetTime()`, on the dimmed overlay covering the browser (no interaction while scanning).
- **Start path** — desktop starts at `std::filesystem::current_path()`, Switch starts at `sdmc:/` (both inside the local source). (TODO_6 later overrides this with `default_folder` when set.)
- Lists folders; ".." pinned on top; shows file size and file type; icon per entry; only playable files shown (unknown extensions filtered out).

## FileEntry — src/filesystem/FileEntry.h

```cpp
struct FileEntry {
    std::string name;
    std::int64_t file_size;   // bytes (was int32 "Kb"); Gui formats B/KB/MB
    std::string type;         // uppercase extension without dot ("S3M"), "Folder", or "Source" (virtual-root entries)
    bool is_directory;
};
```

## DataSource — src/filesystem/DataSource.h (header-only interface)

```cpp
class DataSource {
public:
    DataSource() = default;
    DataSource(const DataSource &) = delete;
    DataSource &operator=(const DataSource &) = delete;
    virtual ~DataSource() = default;

    [[nodiscard]] virtual std::string getDisplayName() const = 0;         // "Local files", "Modland (FTP)"
    [[nodiscard]] virtual std::filesystem::path getRootPath() const = 0;  // top of the source; ".." here exits to the virtual root

    // Blocking. Raw entries (name, byte size, is_directory) of one directory, hidden entries already
    // skipped; no filtering/sorting (FileSystem applies isPlayable, type derivation, and sort).
    // nullopt = hard failure (already SDL_Logged) -> FileSystem keeps the current listing.
    [[nodiscard]] virtual std::optional<std::vector<FileEntry>> listDirectory(const std::filesystem::path &path) = 0;

    // Blocking. Resolves a source path to a locally-openable file for PlayerController::play().
    // Local source: returns the path itself. Remote sources: download to cache, return the cache path.
    // Empty path = failure (already SDL_Logged).
    [[nodiscard]] virtual std::filesystem::path fetchFile(const std::filesystem::path &path) = 0;
};
```

Paths stay `std::filesystem::path` throughout: on both targets (Linux, devkitPro newlib) it is POSIX-flavored, so remote paths like `/pub/modules/AHX` round-trip exactly, `operator/`/`filename()` behave, and the path-based player API needs no conversions.

**Serialization contract**: FileSystem guarantees at most one DataSource call is in flight at any time — `listDirectory`/`fetchFile` run either on the worker thread or (TODO_4's inline fetch) on the main thread while no worker runs. Sources need no internal locking.

## LocalDataSource — src/filesystem/LocalDataSource.{h,cpp}

```cpp
class LocalDataSource final : public DataSource {
public:
    std::string getDisplayName() const override;         // "Local files"
    std::filesystem::path getRootPath() const override;  // "/" desktop, "sdmc:/" under __SWITCH__
    std::optional<std::vector<FileEntry>> listDirectory(const std::filesystem::path &path) override;
    std::filesystem::path fetchFile(const std::filesystem::path &path) override { return path; }
};
```

`listDirectory`: `std::filesystem::directory_iterator(path, skip_permission_denied, ec)` — on error, `SDL_Log` and return what was readable (partial is fine — never nullopt for the local source). Skip hidden entries (dot-prefixed). Sizes via the `error_code` overloads.

## FileSystem — src/filesystem/FileSystem.{h,cpp} (threaded)

Two-phase init matching codebase convention (`create()/destroy()`):

- `void create(std::vector<std::unique_ptr<DataSource>> sources, const std::filesystem::path &startPath, std::function<bool(const std::filesystem::path &)> isPlayable)` — stores the sources (`sources[0]` is the startup source, activated immediately) and the predicate, launches the initial scan of `startPath`.
- `void destroy()` — joins the worker. **main.cpp ordering: `file_system.destroy()` BEFORE `player.destroy()`** — the predicate calls into PlayerController from the worker thread.
- `void navigateToEntry(const FileEntry &entry)` — ignored if a scan is running (UI is blocked by the overlay anyway). At the virtual root: match `entry.name` against the source display names, set the active source, scan its `getRootPath()`; if that scan fails (nullopt — e.g. server down), `update()` keeps the virtual-root listing and resets the active source (user stays put, SDL_Log explains). Inside a source: joins the previous finished worker, then launches a `std::thread` scanning `m_path / entry.name`.
- `void navigateToParent()` — at the virtual root: no-op. At the source root (`m_path == m_activeSource->getRootPath()`): clear the active source and show the virtual root. Otherwise scan `m_path.parent_path()`.
- `void requestFile(const FileEntry &entry)` — resolves the entry to a playable local path via `m_activeSource->fetchFile(m_path / entry.name)` and stores a `FetchResult{localPath, succeeded}`. **TODO_4: called inline on the main thread** (local fetch is identity — instant); TODO_7 moves it onto the worker behind the spinner. Ignored while a scan is running.
- `std::optional<FetchResult> consumeFetchResult()` — consume-once, polled from `Application::update()` (same pattern as `consumeTrackEnded()`).
- `void update()` — called once per frame (from `Application::update()`): if a finished scan is pending, swap it into `m_path`/`m_content` (main thread only).
- Getters: `getPath()` (**empty path at the virtual root**), `getContent()`, `isWorking()`.

**Virtual root**: `m_activeSource == nullptr` means "at the sources list". `m_content` is then one `FileEntry{source->getDisplayName(), 0, "Source", true}` per source, built synchronously on the main thread (nothing to scan) — the one exception to "m_content mutated only in update()", still main-thread-only and guarded by `!m_working`, so the "worker never touches m_content" contract holds.

Threading contract (same style as PlayerController, to be documented in docs/filesystem.md):

| Data | Protection |
|---|---|
| `m_path`, `m_content` (refs handed to Gui) | mutated only on the main thread (`update()`, or the synchronous virtual-root swap while `!m_working`) |
| `m_pendingPath`, `m_pendingContent` | `m_mutex` (worker writes, `update()` swaps) |
| `m_working` | `std::atomic_bool` (worker clears; Gui overlay + navigation/request guards read) |
| `m_sources` | immutable after `create()` |
| `m_activeSource` | main thread only, mutated only while the worker is idle (navigation guard); the worker uses the source captured at launch |
| `m_fetchResult` | `m_mutex` (TODO_4: written inline on the main thread; TODO_7: worker writes, `consumeFetchResult()` takes) |
| `m_isPlayable` predicate | immutable after `create()`; called on worker thread — safe: `PlayerController::isSupported()` only reads plugin extension lists that are immutable while the device is open |

Worker scan: `m_activeSource->listDirectory(path)` returns raw entries; FileSystem then drops files failing `m_isPlayable(name)`, derives `type` (uppercase extension / "Folder"), and sorts: directories first, then files, case-insensitive by name. `nullopt` from the source → keep the current listing. `".."` is NOT an entry — the Gui pins it.

## Gui — src/gui/Gui.{h,cpp}

- **Table gains a Type column**: Name (stretch) | Type (fixed) | Size (fixed). Size formatted from bytes (`B` / `KB` / `MB` with one decimal). Existing clipper and scroll-freeze kept; TODO_3's restyle (no outer borders, hover highlight) kept.
- **Icons per entry** (existing glyph style): folder glyph (amber) for directories, ".." and source entries, music-note glyph (blue) for files — unchanged glyphs, now driven by real data; `entry.type` shown in the Type column.
- **Directory clicks wired**: new `UiActions::onDirectoryClick` member (`std::function<void(const FileEntry &)>`), filling the three `// Click` stubs — the ".." row invokes it with a synthetic `FileEntry{"..", 0, "Folder", true}`.
- **Spinner overlay** (replaces the ProgressBar in the `isWorking` block): keep the 0.8-alpha window covering the table; center `"%c  Scanning..."` where the char cycles `| / ─ \` via `static_cast<int>(ImGui::GetTime() * 8) % 4`. Wrap the table in `ImGui::BeginDisabled(isWorking)`/`EndDisabled` so keyboard/gamepad nav is blocked too, not just the mouse.

## Application / main.cpp

- **`src/Application.{h,cpp}`** — new `void handleDirectoryClick(const FileEntry &entry)`: `entry.name == ".."` → `file_system.navigateToParent()`, else `file_system.navigateToEntry(entry)` (no path joining in Application — at the virtual root, names are display names, not path components); wired into `makeUiActions()`.
- **Playback goes through FileSystem** (so TODO_7 can make resolution asynchronous without touching callers): file click → `player.isSupported(entry.name)` check stays, then `m_advanceDirection = 0; file_system.requestFile(entry);` instead of `player.play(join)`. `playAdjacentTrack(direction)` requests only the *first* playable sibling candidate and records it in `m_lastRequestedName` (the retry cursor); `m_advanceDirection` keeps the direction (±1, or 0 for a direct click).
- **`Application::update()`** gains, next to the track-ended poll: `file_system.update();` and the consume-and-play block:
  ```cpp
  if (auto r = file_system.consumeFetchResult()) {
      if (r->succeeded && player.play(r->localPath)) { m_lastRequestedName.clear(); }
      else if (m_advanceDirection != 0) playAdjacentTrack(m_advanceDirection);  // skip broken sibling
      // direct click failure (direction 0): SDL_Log only
  }
  ```
  `playAdjacentTrack` searches from `m_lastRequestedName` when set, else from `player.getCurrentPath().filename()`. This supersedes TODO_1's verbatim move of the play loop — the retry loop lives at the consume site because success is no longer known at request time. Net behavior for local files is identical to today, plus one frame of latency.
- **`makeUiState()`** maps the empty path to the label `"Sources"` (view-model translation belongs in Application, not FileSystem or Gui).
- **`src/main.cpp`** — start path: `#if defined(__SWITCH__)` → `"sdmc:/"`, else `std::filesystem::current_path()`. After `player.create()`: build `std::vector<std::unique_ptr<DataSource>>` holding one `LocalDataSource`, then `file_system.create(std::move(sources), startPath, [](const std::filesystem::path &p) { return player.isSupported(p); });`. `finalize()`: `file_system.destroy();` first (join worker), then `player.destroy()`.
- The `TODO(temporary)` hardcoded test track on PLAY-when-STOPPED stays (separate cleanup).

## Docs

- **`docs/filesystem.md`** — new classDiagram (`DataSource` interface + `LocalDataSource` realization, FileSystem create/destroy/navigateToEntry/requestFile/update, FileEntry.type) + the threading-contract table, the virtual-root state note, the DataSource serialization contract, and the requestFile/consumeFetchResult flow; remove the "placeholder" note.
- **`docs/ui.md`** — 3-column browser, spinner overlay, `onDirectoryClick`.
- **`docs/application.md`** — handleDirectoryClick, the play-request flow (requestFile / consume / retry cursor), update() responsibilities.

CMakeLists.txt: add `src/filesystem/LocalDataSource.cpp` to `add_executable`.

## Coordination

- **Requires TODO_1** (UiActions member + Application home for navigation logic); assumes TODO_3's restyled left pane.
- **TODO_6** later feeds `default_folder` into the start path — keep the start-path selection in one spot in main.cpp so it's a one-line change.
- **TODO_7** implements `FtpDataSource` against the `DataSource` interface and moves `requestFile` onto the worker; do not add source-type conditionals anywhere outside FileSystem/main.cpp.

## Switch notes / risks

- `std::thread` and `std::filesystem` work on devkitPro (newlib + pthread); `sdmc:/` paths flow through `std::filesystem::path` as plain strings. `LocalDataSource::getRootPath()` returns `sdmc:/` under `__SWITCH__` (note: `parent_path()` of `sdmc:/` returns itself, so root detection uses `getRootPath()` comparison, not `parent_path()`). The Switch build must compile (`cmake-build-switch`); real SD-card behavior needs hardware.

## Verification

- Desktop + Switch builds green (per CLAUDE.md).
- Run `./build/OSP2` from repo root: repo listing shows only folders + playable files with Type/Size columns; enter `romfs/music`, click a track → plays; ".." climbs to `/`; one more ".." shows the sources list ("Local files"); entering it lands at `/`; unreadable dir (e.g. `/root`) logs and shows what was readable without crashing.
- Click-to-play and auto-advance behave exactly as before (now routed through requestFile/consumeFetchResult).
- Spinner: temporarily add a `sleep_for` in the worker to see the overlay + spinner and confirm clicks are blocked while scanning; remove before finishing.
- End-of-track auto-advance still works with real sibling listings (`playAdjacentTrack` now walks real entries).
