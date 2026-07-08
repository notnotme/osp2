# TODO_35 — No overlay blink between "Downloading..." and "Loading..."

> Direct-click playback of a non-cached remote file shows the "Downloading..." overlay during the FTP fetch, then the overlay vanishes for a frame (or a few) before the decode's "Loading..." overlay comes up. Keep one continuous overlay across the fetch → decode hand-off. Observed on Switch hardware while verifying TODO_31. **Not** covered by TODO_22, which fixed the next/prev *boundary* blink (reload continuity + `m_advanceLoadInFlight` suppression) — this is the direct-click hand-off seam, where both overlays are intentionally shown.

## Context / analysis

- `Application::makeUiState()` (`src/Application.cpp:383-417`) reads `m_fileSystem.isWorking()` **live** (`:393`) — the atomic `m_working` the fetch worker clears from its own thread. The gap frame: `Application::update()` polls `m_fileSystem.update()` while `m_working` is still true (no join, result not yet visible), the worker then finishes, and `makeUiState()` later that same frame reads `isWorking() == false` → no overlay, even though the finished fetch's result is parked unconsumed (`m_fetchResult`). Next frame `update()` joins, `consumeFetchResult()` feeds `m_player.play()`, `isLoading()` goes up, and "Loading..." appears — one blank frame (or more at low fps) in between.
- The consume-frame itself is seamless: `consumeFetchResult()` → `play()` (which raises `m_loading` synchronously) happens in `Application::update()` *before* `makeUiState()` (`src/Platform.cpp` frame loop), so once the result is consumed the overlay switches "Downloading..." → "Loading..." without a gap. Only the finished-but-unconsumed window blinks.
- TODO_22's `m_advanceLoadInFlight` only *suppresses* "Loading..." during auto-advance; a direct click keeps both overlays and therefore shows the seam.

## Fix directions (decide at implementation)

- **Application-side latch (preferred)**: treat the hand-off as still-working in `makeUiState()` — keep "Downloading..." up while a finished fetch is parked unconsumed, or latch from the fetch request until the decode's `m_loading` is up / the fetch resolves failed-or-cancelled. The overlay MUST still clear promptly on a cancelled or failed fetch (cancel → silent; direct-click failure → error popup, TODO_17b), and must not linger at track end.
- **FileSystem-side alternative**: make `isWorking()` also report true while a finished `Fetch` result is parked (`m_workKind == Fetch` + `m_fetchResult` set). Riskier: `isWorking()` gates navigation and `requestFile*` re-entry, and `m_workKind`'s main-thread contract says it is only accurate alongside a live worker — audit all callers before widening the semantics.

## Task chunks (implement, verify, and commit one at a time)

- [ ] **35 — Continuous overlay across the fetch→decode hand-off**: direct click from the browser *and* playlist replay (`requestFileFromSource`) show an unbroken overlay "Downloading..." → "Loading..."; a cancelled or failed fetch still drops the overlay the frame it resolves; TODO_22 auto-advance semantics (no overlay for seamless local advance, "Downloading..." for remote siblings) unchanged.

## Files to change

1. **`src/Application.{h,cpp}`** — hand-off latch in `update()` / `makeUiState()` (preferred direction).
2. **`src/filesystem/FileSystem.{h,cpp}`** — only if the FileSystem-side option is chosen instead.

## Docs

- **`docs/application.md`** — overlay decision notes (or `docs/filesystem.md` if `isWorking()` semantics change).

## Coordination

- Independent of TODO_32–TODO_34 (touches the `makeUiState` overlay logic only; TODO_32a moves the visualizer bridge into `Application` — same file, trivial merge either order).

## Verification

- Desktop + Switch builds green.
- Click a non-cached remote file → one continuous overlay, label switching "Downloading..." → "Loading..." with no blank frame (most visible on Switch hardware).
- Same for playing a remote playlist entry while browsing elsewhere.
- Cancel mid-download → overlay clears immediately, no chained advance; failed download → overlay clears + error popup.
- Local files and auto-advance behave exactly as before (TODO_22 checks still pass).
