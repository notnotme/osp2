# TODO_34 — Player plugins: RAII lifecycle + PluginUtil dedup

> From the 2026-07 architecture audit. (1) The decoder plugins' two-phase `create(sampleRate)`/`destroy()` buys nothing: construction and `create()` are adjacent (`PlayerController.cpp:52-60`), no `create()` reports failure, and the split's only present-tense product is SidPlugin's defensive double-join. Collapse into constructor/destructor per the project's own no-two-phase-init-except-platform-boundaries rule. (2) Three utility fragments are triplicated across the plugins and already drifting — dedupe into a header-only `PluginUtil.h`. Independent of every other TODO.

## Context

- **RAII collapse scope — critical asymmetry:**
  - `PlayerPlugin::create/destroy` (`src/player/PlayerPlugin.h:41-42`): **collapse.** Per plugin, `create()` is store-rate + fetch-extensions (`OpenMptPlugin.cpp:56-59`, `GmePlugin.cpp:84-87`), plus engine/ROMs/DB-thread (`SidPlugin.cpp:123-138`), plus `sc68_init`/`sc68_create` with failure handled *internally* by leaving `m_sc68` null so `open()` fails defensively (`Sc68Plugin.cpp:78-105`). No caller-visible failure → nothing changes semantically in a constructor.
  - `VisualizerPlugin::create/destroy` (`src/visualizer/VisualizerPlugin.h:39-40`): **do NOT touch** — its two-phase lifecycle is load-bearing: `create()` requires a live GL context (`Platform.cpp:58-60` sequences it after GL init, but the controller is constructed *before* GL exists) and `destroy()` must free GL objects while the context is still valid (`Platform.cpp:180-181`). Optional minor cleanup only: give them default empty bodies instead of pure-virtual so ImGui-only plugins like `BarsVisualizer` stop stubbing them.
  - `PlayList::create()/destroy()` (`src/playlist/PlayList.cpp:28-35`): literally empty, "kept for symmetry" — **collapse too** (remove the calls at `Platform.cpp:61, 182`).
- **Destructor rule:** each plugin destructor calls its own teardown directly, **not** the virtual `close()` — destructors must not rely on virtual dispatch; each current `destroy()` body already resets the decoder object, which releases any open track. `SidPlugin`'s destructor keeps exactly one join (the current `destroy()` body, `SidPlugin.cpp:159-164`); delete the defensive duplicate (`:114-121`).
- **Ordering unchanged:** everything still runs on the main thread at the same points. `PlayerController::destroy()` keeps closing the device first (`PlayerController.cpp:89-92`); `m_plugins.clear()` then destroys. The Sid DB loader thread starts at construction — the same instant `create()` runs today.
- **Triplicated fragments (drift confirmed):**
  - `toString(const char *)` — verbatim ×3: `GmePlugin.cpp:57-59`, `SidPlugin.cpp:54-56`, `Sc68Plugin.cpp:45-47`.
  - Read-whole-file-into-bytes — ×3 with drifted shapes: `GmePlugin.cpp:97-103`, `SidPlugin.cpp:199-204`, `Sc68Plugin.cpp:132-147` (the sc68 copy grew its own empty-file check the others lack).
  - ASCII-lowercase-with-unsigned-char-cast — ×3 in the player domain: `GmePlugin.cpp:63-69`, `PlayerController.cpp:434-436`, `SongLengthDb.cpp:78-80`. **Scope rule:** player domain only — `FileSystem::deriveType` *uppercases* for display and stays as-is; no cross-domain util header.
  - Also drifted: `OpenMptPlugin::getTitle()` reads the module live (`OpenMptPlugin.cpp:115-117`) where the other three serve an open()-cached `m_title` — contract-compliant (only called under the lock) but worth unifying: cache in `open()`.
- **Audited and rejected — do not do instead:** a Template Method base over the four `open()`s (the apparent shared skeleton dissolves — each library's init is genuinely different); a `PluginRegistry<T>` unifying Player/Visualizer controllers; a table-driven `applySetting` dispatcher (would hide the per-key live/deferred policies the comments make explicit).

## Task chunks (implement, verify, and commit one at a time)

- [x] **34a — RAII collapse**: delete the two pure virtuals from `PlayerPlugin.h:41-42` (document that implementations take the sample rate as a constructor parameter and fully tear down in their destructor); per plugin, move the `create()` body into `explicit XxxPlugin(int sampleRate)` and the `destroy()` body into the destructor; `PlayerController::create()` becomes `m_plugins.emplace_back(std::make_unique<OpenMptPlugin>(SAMPLE_RATE));` etc. (drop the create loop, `PlayerController.cpp:52-60`); `PlayerController::destroy()` keeps the `close()` loop, drops the `destroy()` calls (`:94-98`). Collapse `PlayList::create()/destroy()` (remove `Platform.cpp:61, 182` calls). Optional: default-empty `VisualizerPlugin::create/destroy` bodies + remove `BarsVisualizer` stubs.
- [x] **34b — PluginUtil.h + statusLocked**: new header-only `src/player/plugins/PluginUtil.h` (GPL header, no CMake change) with inline `std::string toString(const char *)`, `std::string asciiToLower(std::string_view)`, and `std::optional<std::vector<char>> readFileBytes(const std::filesystem::path &)` (nullopt when the file can't open; does **not** catch — each plugin keeps its outer must-not-throw try, which also covers library calls; callers keep their own `SDL_Log` prefixes). Replace the nine sites (`PlayerController.cpp` and `SongLengthDb.cpp` may include it — same domain). Fold-ins: cache `m_title` in `OpenMptPlugin::open()`; optionally move the duplicated `static_assert(sizeof(short) == sizeof(std::int16_t))` (`GmePlugin.cpp:41-44`, `SidPlugin.cpp:40-43`) into the header; extract `PlayerController::statusLocked(PlayerState stateOverride)` (precondition: caller holds `m_mutex`) deduping the snapshot construction in `play()` (`PlayerController.cpp:118-133`) vs `getStatus()` (`:323-346`) — mind their deliberate stopped-zeroing difference. Leave the two local `CHANNELS = 2` constants (`SidPlugin.cpp:47`, `Sc68Plugin.cpp:37`) — deliberate decoupling, already pinned by static_asserts.

## Files to change

1. **`src/player/PlayerPlugin.h`** — drop `create`/`destroy` virtuals (34a).
2. **`src/player/plugins/{OpenMptPlugin,GmePlugin,SidPlugin,Sc68Plugin}.{h,cpp}`** — ctor/dtor lifecycle (34a); PluginUtil call sites + OpenMpt title cache (34b).
3. **`src/player/PlayerController.{h,cpp}`** — construction/destruction sites (34a); `asciiToLower` site + `statusLocked` (34b).
4. **`src/playlist/PlayList.{h,cpp}`**, **`src/Platform.cpp`** — drop empty lifecycle (34a).
5. **`src/visualizer/VisualizerPlugin.h`** (+ `BarsVisualizer`) — optional default bodies (34a).
6. **`src/player/plugins/PluginUtil.h`** (new, header-only) (34b).
7. **`src/player/SongLengthDb.cpp`** — `asciiToLower` site (34b).

## Docs

- **`docs/audio.md`** — Mermaid diagram: drop `+create(sampleRate)*`/`+destroy()*` from `PlayerPlugin` and the controller's create/destroy notes; update the "adding a decoder" recipe and the SidPlugin/Sc68Plugin `create()` mentions; one line on `PluginUtil.h` in the recipe.
- **`docs/playlist.md`** — drop the empty lifecycle if diagrammed.

## Coordination

- Independent of TODO_31–TODO_33. Most churn per visible change of the batch — do last.

## Verification

- Desktop + Switch builds green each chunk.
- 34a: full playback regression pass — one file per format (mod/xm, nsf/spc, sid, sndh), subtrack NEXT/PREVIOUS on a multi-track file, plugin settings popup applies live and deferred settings, cancel-mid-load, SID durations still resolve from the Songlengths DB (proves the loader thread + join survived the move), clean exit (no hang = joins correct).
- 34b: same formats still load (file-read path swapped), GME Shift-JIS and SID/sc68 Latin-1 metadata still transcode, OpenMPT title still shows.
