# TODO_31 — Filesystem robustness: cancellable playlist-replay fetches + FTP listing dedup

> Two filesystem fixes from the 2026-07 architecture audit. (1) `FileSystem::cancel()` targets `m_activeSource`, so a playlist-replay download from a *non-browsed* remote source cannot be cancelled — the overlay's Cancel button is a silent no-op and the UI stays modal until the transfer completes. The code marks this "acceptable this round" (`FileSystem.cpp:163-164`); since TODO_28e made cross-source playlist fetches routine, the round is over. (2) `FtpDataSource` carries two ~30-line verbatim copies of the listing-splitter loop. Independent of every other TODO.

## Context

- `requestFileFromSource` (`src/filesystem/FileSystem.cpp:153-171`) deliberately does **not** touch `m_activeSource`/`m_path` (a playlist entry may live in a different source than the browsed one), and hands the worker `m_sources[sourceIndex].get()` directly. `cancel()` (`:173-180`) only signals `m_activeSource`; its comment "Navigation is blocked while working, so m_activeSource is exactly the source the worker is using" is now **false** — `requestFileFromSource` contradicts it.
- The cross-thread part of cancellation is already sound and stays untouched: `DataSource::cancel()` sets an atomic (`FtpDataSource.cpp:602-604`), curl's progress callback aborts (`:39-41`), and the flag is cleared at the start of every operation (`:451, 536`). Only *which source* gets the signal is broken.
- `parseMlsdListing` (`src/filesystem/FtpDataSource.cpp:137-166`) and `parseListListing` (`:234-263`) duplicate the line-iteration loop verbatim (find `\n`, strip `\r`, skip empties, switch on `LineOutcome`, log malformed); only the per-line parser (`parseMlsdLine` vs `parseListLine`) and the log label differ. The comment at `:232-233` even says "mirroring parseMlsdListing".
- The MLSD and LIST transfer setups (`:468-475` vs `:500-506`) also share ~8 lines (reset, `applyCommonOptions`, URL, write-to-string, perform).
- **Do NOT restructure** the MLSD → LIST → stale-cache fallback ladder or split `FtpDataSource` into transport + cache proxy — audited and rejected: the cache policy is entangled with transport-specific failure classification (`isConnectionFailure`, cancel-vs-network-failure fallback decisions) that a generic wrapper can't see through the `DataSource` interface.

## Task chunks (implement, verify, and commit one at a time)

- [x] **31a — Work-source cancel fix**: add `DataSource *m_workSource = nullptr;` to `FileSystem` (`src/filesystem/FileSystem.h`, alongside `m_workKind`; main-thread only — written before each worker start, read by `cancel()` on the main thread; the cross-thread part remains `DataSource::cancel()`'s own atomic). Set it in `startScan` (`FileSystem.cpp:277-286`), `startFetch` (`:288-297`), and `requestFileFromSource` (`:153-171`); clear it where `m_workKind` returns to `None` in `update()` (`:206, :245`). `cancel()` becomes `if (m_working.load() && m_workSource != nullptr) m_workSource->cancel();`. Delete the known-limitation comment (`:163-164`) and fix the now-false claim in `cancel()`'s comment (`:175-176`). Optional fold-in: `FetchResult{path, bool}` (`FileSystem.h:38-41`) → `std::optional<std::filesystem::path>`, unifying the failure encoding with `listDirectory`.
- [ ] **31b — FTP listing-splitter dedup**: in `FtpDataSource.cpp`'s anonymous namespace, one `std::vector<FileEntry> parseListing(const std::string &response, LineOutcome (*parseLine)(const std::string &, FileEntry &), const char *formatLabel)`; `parseMlsdListing`/`parseListListing` become one-line wrappers (or the two call sites call it directly). Optional fold-in: a `CURLcode performListingTransfer(const std::string &url, const char *customRequest, std::string &response)` helper collapsing `:468-475`/`:500-506`. No behavior change; the cancel/fallback decision logic stays exactly where it is.

## Files to change

1. **`src/filesystem/FileSystem.{h,cpp}`** — `m_workSource` member + set/clear/cancel sites (31a).
2. **`src/filesystem/FtpDataSource.cpp`** — `parseListing` + optional transfer helper (31b).

## Docs

- **`docs/filesystem.md`** — remove/replace the known-limitation note about uncancellable playlist fetches (31a). No diagram topology change (private members/helpers).

## Coordination

- Independent of TODO_32–TODO_34; ranked first on value (fixes a real user-facing defect).

## Verification

- Desktop + Switch builds green each chunk.
- 31a: browse a remote source, start a scan, Cancel works as before; add a remote entry to the playlist, browse a *different* source, play the playlist entry, hit Cancel mid-download → the transfer aborts and the overlay clears (previously impossible). Cancel during a normal browser-initiated download still works.
- 31b: remote browsing unchanged — MLSD listings, the LIST fallback, and cached listings all render identically.
