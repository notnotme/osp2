# TODO_17 — Playback loading overlay + error notifications

> Two chunks covering the two gaps in the play path: big files freeze the UI because decode runs synchronously on the UI thread (17a), and playback failures are silently swallowed with nothing shown to the user (17b). Benefits from TODO_12's cleaner `main.cpp`/paths; otherwise independent.

## Context

**Loading (17a).** The fetch/download step is already async on a `FileSystem` worker thread, covered by the existing overlay — spinner + dimmed backdrop + Cancel — in `Gui::drawFileBrowser` (`src/gui/Gui.cpp:462–510`), driven by `FileSystem::isWorking()` / `workingLabel` in `UiState`. **But the actual decode is synchronous on the UI thread**: `Application::update()` (`src/Application.cpp:160`) calls `PlayerController::play()` (`src/player/PlayerController.cpp:90`), which under `m_mutex` calls `plugin->open()` — e.g. `OpenMptPlugin::open` reads and parses the whole module inline. A large/slow module therefore freezes the UI for the entire parse, with no overlay (item #2: "big files take time to load on Switch and block UI"). A blocking overlay alone can't fix this — ImGui cannot render while the UI thread is parsing — so the real fix is to move the decode off the UI thread and let the existing overlay cover it.

**Errors (17b).** `PlayerController::play()` returns `bool` (`PlayerController.h:75`): `false` on no matching plugin (`:92`) or on `plugin->open()` failure (`:105–107`). Plugin failures only `SDL_Log` (e.g. `OpenMptPlugin::open` "cannot open" / "failed to parse"). At the call site (`Application.cpp:166–173`) a direct-click failure is **silently swallowed** (comment: "SDL_Log inside the player is enough"); auto-advance only skips a broken sibling. `UiState` has **no error field**, and there is no toast/notification anywhere. Modal-popup infrastructure to reuse exists: the About modal (`Gui.cpp:283`, latched via `m_aboutRequested`, opened in the menu-bar scope `Gui.cpp:266–277`) and the per-plugin settings popups.

## Task chunks (implement, verify, and commit one at a time)

- [x] **17a — Async decode + loading overlay**: move `PlayerController::play()` / `plugin->open()` off the UI thread — extend the existing `FileSystem` worker path (fetch → then decode on the same worker) or add an equivalent worker in the player — and surface a "Loading…" working state into `UiState` so the **existing** overlay (`Gui.cpp:462–510`) covers the decode too, not just the download. Honour the threading contract: `open()`/`decode()` run under `PlayerController::m_mutex`; track teardown never happens on the audio thread; `destroy()` still closes the device before destroying plugins. Reuse `UiState::isWorking`/`workingLabel` (add a distinct "Loading…" label) rather than inventing a second overlay. Verify on Switch hardware with a large module: the UI stays responsive and shows the spinner during load, then playback starts; Cancel during load aborts cleanly.
- [ ] **17b — Playback error notifications**: give `play()` a richer result than `bool` (distinguish *unsupported extension* from *decode/parse failure*; also surface fetch failure `r->succeeded == false`), capture the reason in the failure branch of `Application::update()` (`Application.cpp:169–172`), add an error/notification field to `UiState`, and render it via a modal reusing the About/plugin popup pattern (a latch member on `Gui` like `m_aboutRequested`, `OpenPopup` in the menu-bar scope, `BeginPopupModal` + Close). Keep auto-advance behaviour (a broken sibling is skipped, not popped up per file — decide whether auto-advance failures are silent or coalesced). Verify: clicking an unsupported or corrupt file shows a clear message ("Cannot play <file>: unsupported format" / "…: failed to decode") instead of failing silently; a fetch failure is also reported; normal playback shows no popup.

Each chunk ends with green desktop + Switch builds, docs updated (`docs/audio.md`, `docs/ui.md`, `docs/application.md` as touched), user verification (17a needs a Switch hardware test), then a commit. Run cpp-reviewer on the diff before committing.

## Architecture

```
selectfile → Application::handleFileClick → FileSystem::requestFile (worker: fetch)
                                                     │  17a: worker continues → decode
                                                     ▼
main thread: Application::update()  ── isWorking/"Loading…" ──▶ existing overlay (spinner+cancel)
                    │ on result
                    ├─ success ─────────────────────────────▶ playback
                    └─ failure(status) ─17b─▶ UiState.error ─▶ Gui error modal (About-style popup)
```

## Files to change

1. **`src/player/PlayerController.{h,cpp}`** — richer `play()` status (e.g. `enum class PlayResult { Ok, Unsupported, DecodeError }`); support decode being driven from a worker while preserving the `m_mutex` contract.
2. **`src/Application.{h,cpp}`** — drive/await the async decode; on failure map the status to a user message; keep the auto-advance skip.
3. **`src/gui/UiState.h`** — a "Loading…" working state (17a) and an error/notification field (17b).
4. **`src/gui/Gui.{h,cpp}`** — extend the existing overlay to the loading state (17a); add an error modal latch + renderer reusing the About-popup pattern (17b).
5. **`src/filesystem/FileSystem.{h,cpp}`** — only if the decode is chained onto the existing fetch worker.

No new external dependency; CMakeLists.txt change only if a new `.cpp` is added.

## Docs

- **`docs/audio.md`** — the `play()` status result and the async-decode threading (decode on a worker, `open()` under `m_mutex`, teardown off the audio thread).
- **`docs/ui.md`** — the loading state reusing the browser overlay, and the new error modal.
- **`docs/application.md`** — the failure-handling flow (status → `UiState.error`), and how it coexists with auto-advance skipping.

## Coordination

- Benefits from **TODO_12** (cleaner `main.cpp`/paths) but not blocked by it. Reuses the overlay from **TODO_4/TODO_7** and the modal pattern from **TODO_3/TODO_6**. Independent of TODO_14/15/16.

## Verification

- Desktop + Switch builds green (per CLAUDE.md).
- **17a**: on Switch hardware (ask before deploying) load a large module — the spinner shows and the UI stays responsive during decode, then playback starts; Cancel mid-load aborts without crashing; the audio-thread mutex/teardown contract is respected (no data race, clean stop).
- **17b**: click an unsupported extension → "unsupported format" popup; click a truncated/corrupt module → "failed to decode" popup; a fetch failure is reported too; successful playback shows no popup; auto-advance across a broken sibling still skips silently (or coalesces) rather than spamming popups.
