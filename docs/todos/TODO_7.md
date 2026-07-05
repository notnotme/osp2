# TODO_7 — Remote data source: Modland FTP via libcurl

> Requires TODO_4 (`DataSource` interface, virtual root, requestFile pipeline). Independent of TODO_5/TODO_6; sequenced last. Optional chunk 7d needs TODO_6's Settings.

## Task chunks (implement, verify, and commit one at a time)

- [x] **7a — curl dependency + FTP listing**: install deps (`sudo apt install libcurl4-openssl-dev`; `sudo dkp-pacman -S switch-curl` — pulls switch-mbedtls/switch-zlib), CMake libcurl stanza (identical on both platforms), curl/socket lifecycle in main.cpp, new `src/filesystem/FtpDataSource.{h,cpp}` registered as "Modland (FTP)" with `listDirectory` implemented via MLSD; `fetchFile` stubbed (SDL_Log "not implemented", returns empty path). Verify: virtual root shows two sources; browse the Modland format/artist tree with correct names (spaces!) and sizes; only playable extensions listed; network down → entering/navigating logs and stays put; both builds green.
- [x] **7b — Download-to-cache + playback**: `FtpDataSource::fetchFile` (cache-hit check, download to `.part`, rename on success); `FileSystem::requestFile` moves onto the worker with `m_working = true` for ALL sources — one uniform path, the 4c spinner now covers downloads. Verify: click a Modland track → brief spinner → plays; second click instant (cache hit, file present under `cache/modland/pub/modules/...`); next/prev/auto-advance walk the FTP listing correctly.
- [x] **7c — Robustness + polish**: "Scanning..." vs "Downloading..." overlay label, cache-path sanitization (+ `.`/`..` traversal guard), timeout tuning (stall 30→15 s), a **Cancel button** on the overlay to abort a stuck scan/download (curl `XFERINFOFUNCTION` + atomic flag; cancelled download doesn't auto-advance; cancelled scan stays put), fetch-failure UX (auto-advance skips the broken sibling; direct click just logs), docs pass. **Switch hardware test still pending** (ask the user first — netloader): browse/play Modland over Wi-Fi, confirm the Cancel button is gamepad-reachable, confirm the TTL clock behaves. Desktop verified: cancel aborts a stuck scan/download and stays put/doesn't chain; labels correct; both builds green.
- [x] **7d (optional, requires TODO_6) — User-defined FTP sources**: `[source.NAME]` host/path sections in the INI parsed in main.cpp into extra `FtpDataSource` instances; adds a LIST-parse fallback for servers without MLSD.

Each chunk ends with green desktop + Switch builds, docs updated (`docs/filesystem.md`, `docs/ui.md`, `docs/application.md` as touched), user verification, then a commit.

## Context

TODO_4 introduced the `DataSource` interface with `LocalDataSource` as its only implementation. This item adds `FtpDataSource`, making internet chiptune archives browsable and playable straight from the existing file browser. The built-in source is **Modland** (`ftp.modland.com/pub/modules`, anonymous FTP, ~514k modules organized as `<format>/<artist>/<file>`).

- **Protocol layer = libcurl** — system libcurl on desktop, the `switch-curl` devkitPro portlib (7.69.1) on Switch. No hand-rolled FTP; also opens the door to HTTP-based sources later.
- **Playback = download to cache** — `fetchFile` downloads to a local cache file and returns its path; the path-based player/plugin interfaces are untouched, and every future decoder plugin works with remote files for free.
- **UI = nothing new** — the source appears at the virtual root; listings and downloads run behind the TODO_4 spinner overlay (label refined in 7c).
- **Listing format = MLSD** — verified working against ftp.modland.com; machine-readable and robust against Modland's space-filled names ("Fasttracker 2"), where classic `LIST` output would need fragile column parsing.

## FtpDataSource — src/filesystem/FtpDataSource.{h,cpp}

```cpp
class FtpDataSource final : public DataSource {
public:
    FtpDataSource(std::string displayName, std::string host,
                  std::filesystem::path basePath, std::filesystem::path cacheDir);
    ~FtpDataSource() override;                            // curl_easy_cleanup

    std::string getDisplayName() const override;          // "Modland (FTP)"
    std::filesystem::path getRootPath() const override;   // basePath ("/pub/modules")
    std::optional<std::vector<FileEntry>> listDirectory(const std::filesystem::path &path) override;
    std::filesystem::path fetchFile(const std::filesystem::path &path) override;

private:
    CURL *m_curl = nullptr;                               // one reusable easy handle, created lazily
};
```

- **One reusable `CURL*` easy handle** — FTP control-connection reuse makes navigation snappy. Safe without locking because FileSystem serializes all DataSource calls (TODO_4 contract). Created lazily on first use, cleaned up in the destructor.
- **Options set on every call**: `CURLOPT_NOSIGNAL = 1` (mandatory in multithreaded programs), `CURLOPT_CONNECTTIMEOUT = 10`, `CURLOPT_LOW_SPEED_LIMIT = 1` / `CURLOPT_LOW_SPEED_TIME = 30` (stall detection instead of a total timeout, which would kill large files on slow links). Anonymous login and passive mode (EPSV with PASV fallback) are curl's FTP defaults — correct behind NAT on both platforms.
- **URL building**: `ftp://<host>` + per-segment `curl_easy_escape` joined with `/` — Modland paths contain spaces ("Fasttracker 2" must become `Fasttracker%202`).

### listDirectory (MLSD)

`CURLOPT_CUSTOMREQUEST "MLSD"`, URL ending in `/`, response accumulated via `CURLOPT_WRITEFUNCTION`. Per line: the name is everything after the first space; the facts before it split on `;` — `type=` (`dir` → directory, `file` → file, `cdir`/`pdir` → skip), `size=` → bytes. Unparseable line → `SDL_Log` + skip. Transfer failure (timeout, host down, bad path) → `SDL_Log` + `nullopt` — FileSystem keeps the current listing and, when entering the source from the virtual root, stays there.

### fetchFile (download to cache)

1. Compute the cache path (see below); if it exists with size > 0 → cache hit, return it.
2. `std::filesystem::create_directories(parent)`.
3. Download to `<file>.part` (`CURLOPT_WRITEFUNCTION` → `std::ofstream`); on `CURLE_OK` rename to the final name and return it; on failure delete the `.part`, `SDL_Log`, return an empty path.

No numeric download progress — Modland files are a few KB to a few hundred KB (sub-second on any sane link); the spinner + "Downloading..." label suffices and the stall timeout handles pathology. `CURLOPT_XFERINFOFUNCTION` is the extension point if ever needed; the `DataSource` interface stays progress-free.

## Cache

- **Root**: desktop `SDL_GetBasePath() + "cache/"`; Switch `sdmc:/switch/OSP2/cache/`. Per-source subdirectory (`modland/`).
- **Naming mirrors the remote path**: `cache/modland/pub/modules/AHX/Funute/kiteflyer.ahx`. This is load-bearing: it preserves the extension for plugin selection, preserves `filename()` so `playAdjacentTrack`'s name-matching cursor works identically for remote tracks (the player's current path is the cache path, but its filename equals the entry name), is human-inspectable, and makes cache-hit = `exists() && file_size() > 0`.
- **Sanitization (7c)**: FAT-illegal characters in path components (`\ : * ? " < > |`) → `_`, applied on both platforms so desktop and Switch cache layouts stay identical.
- **Eviction: none** — chiptunes are KBs and only clicked files are cached; manual deletion of the cache directory is the documented recourse. A "Clear cache" settings button is a possible TODO_6 follow-up.
- **Adjacent-track prefetch**: explicitly out of scope; noted as future work enabled by this layout.

## FileSystem — requestFile moves onto the worker (7b)

`requestFile` now sets `m_working = true` and runs `m_activeSource->fetchFile(...)` on the worker thread (same join-then-launch pattern as scans); the result lands in `m_fetchResult` under `m_mutex` and is taken by `consumeFetchResult()` on the main thread. Local fetches go through the worker too from now on — one uniform path, no source-type conditionals. Navigation is ignored while a fetch runs (`m_working` guard, UI is behind the overlay anyway). The single worker remains sufficient: all FileSystem work is UI-modal by design, so listing and downloading never need to overlap.

**Overlay label (7c)**: FileSystem gains an atomic work-kind (Scan / Fetch); `UiState` carries the label ("Scanning..." / "Downloading...") to the Gui overlay.

## main.cpp — lifecycle & registration

- `initialize()`: under `__SWITCH__`, `socketInitializeDefault()` first (include `<switch.h>`); then `curl_global_init(CURL_GLOBAL_DEFAULT)` on both platforms — **before `file_system.create()`**, i.e. before any thread exists (curl_global_init is not thread-safe).
- Source registration, next to the `LocalDataSource`:
  ```cpp
  sources.push_back(std::make_unique<FtpDataSource>(
      "Modland (FTP)", "ftp.modland.com", "/pub/modules", cachePath() / "modland"));
  ```
- `finalize()`: `file_system.destroy()` (worker joined, no curl call in flight) → `curl_global_cleanup()` → under `__SWITCH__`, `socketExit()` last.

## CMakeLists.txt

```cmake
# libcurl (FtpDataSource)
pkg_check_modules(libcurl REQUIRED IMPORTED_TARGET libcurl)
target_link_libraries(OSP2 PRIVATE PkgConfig::libcurl)
```

Same stanza on both platforms (per CLAUDE.md: prefer `pkg_check_modules`; switch-curl ships a `libcurl.pc`). Add `src/filesystem/FtpDataSource.cpp` to `add_executable`.

## Docs

- **`docs/filesystem.md`** — classDiagram gains `FtpDataSource` (DataSource realization); notes on MLSD, the cache layout/policy, and curl lifecycle ownership (main.cpp owns global init/cleanup, the source owns its easy handle); requestFile-on-worker update to the threading table.
- **`docs/application.md`** — fetch-failure behavior (auto-advance skips, direct click logs).
- **`docs/ui.md`** — working-label addition (7c).

## Coordination

- **Requires TODO_4** — builds on `DataSource`, the virtual root, and the requestFile/consumeFetchResult pipeline; 7b changes only FileSystem internals (callers untouched, as designed there).
- **TODO_6** — cache location shares the `SDL_GetBasePath()` convention with `osp2.ini`; "Clear cache" and 7d's user-defined `[source.NAME]` sections are the touchpoints.

## Switch notes / risks

- **switch-curl is 7.69.1 (2020)** — use no options introduced after it (everything above is ≤7.32 vintage; no `curl_easy_option_*`).
- **`socketInitializeDefault()` is mandatory** — without it every transfer fails instantly. Call once, pair with `socketExit()`. Desktop is unaffected (no-op branch).
- **TLS is irrelevant here**: Modland is plain anonymous FTP — no mbedtls CA-bundle concerns. Plain FTP is unencrypted; acceptable for a public read-only archive (stated deliberately).
- **Huge directories**: some Modland format dirs have thousands of children — one MLSD transfer of a few hundred KB, a few seconds behind the spinner. Acceptable; noted.
- **LIST-parse variability** is retired by the MLSD decision for Modland; it returns as a risk only for 7d's arbitrary user servers (hence the LIST fallback there).
- Real network behavior on Switch needs hardware (`run_program_on_switch` — ask the user first).

## Verification

- Desktop + Switch builds green (per CLAUDE.md).
- Virtual root shows "Local files" and "Modland (FTP)"; entering Modland lists the format directories; navigate `AHX/<artist>`, names with spaces and sizes correct; only playable extensions shown.
- Click a track → spinner ("Downloading..." after 7c) → plays; re-click → instant (cache hit); cache file exists at the mirrored path.
- Next/previous and end-of-track auto-advance walk the remote listing; a failing download during auto-advance skips to the next sibling.
- Network down: entering the source or navigating logs and stays put; mid-download unplug aborts within the stall timeout without a stuck overlay.
- Switch hardware (with user's go-ahead): browse and play Modland over Wi-Fi.
