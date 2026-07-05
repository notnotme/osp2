# TODO_9 — Additional decoder plugins (`GmePlugin` / `SidPlugin` / `Sc68Plugin`)

> Extends playback beyond tracker modules (libopenmpt) to the three chiptune families the architecture was designed for: **libgme** (NSF/SPC/VGM/GBS/AY/HES/KSS/SAP…), **libsidplayfp** (C64 SID), and **libsc68** (Atari ST YM2149 / SNDH). Each is one `PlayerPlugin` implementation + one `TrackMetadata` variant alternative + one `emplace_back` + one CMake stanza — the recipe `docs/audio.md` already documents and `OpenMptPlugin` already demonstrates. `Metadata.h:44` names all three as the intended additions. Requires TODO_1 (engine), TODO_5 (metadata variant), TODO_6 (plugin settings).

## Context

The decoder layer is deliberately open: `PlayerController` owns `std::vector<std::unique_ptr<PlayerPlugin>>` and dispatches by file extension (`findPluginFor`, `src/player/PlayerController.cpp:237-254`), first-match-wins. The only concrete plugin today is `OpenMptPlugin` (`src/player/plugins/OpenMptPlugin.{h,cpp}`). Three facts from the current code and toolchain shape this item:

- **The metadata variant already reserves these three.** `src/player/Metadata.h:44` — `// Future: GmeMetadata, SidMetadata, Sc68Metadata added with their plugins.` The `std::visit` in `Gui::drawFileMetadata` (`src/gui/Gui.cpp`, ~line 488) has **no `auto` fallback**, so adding a variant alternative is a compile error until the matching `drawXxxMetadata` is written — the compiler is the checklist (per TODO_5).

- **All three libraries emit 16-bit PCM, but `decode()` must fill float.** `PlayerPlugin::decode(float* buffer, int frames)` returns interleaved **stereo float32** (`OpenMptPlugin` gets this for free from `read_interleaved_stereo`). GME (`gme_play` → `short[]`), libsidplayfp (ReSIDfp → 16-bit), and libsc68 all render **16-bit**. Each plugin decodes into a `short` scratch buffer and converts (`sample / 32768.0f`), duplicating mono to L/R where a format is mono.

- **Switch portlibs are the gate.** CLAUDE.md: *"Decoder libraries must be available as devkitPro portlibs (pkg-config)."* Current audit: **`switch-libgme` is installed** (`/opt/devkitpro/portlibs/switch/lib/pkgconfig/libgme.pc`); **`switch-libsidplayfp` and `switch-sc68` are not**, and libsc68 is absent on the desktop dev machine too. Building/installing those portlibs is a **prerequisite handled by the maintainer, not part of a code chunk** (see Prerequisites). Once installed, each plugin follows the single-stanza `pkg_check_modules` pattern for both platforms (mirroring libopenmpt at `CMakeLists.txt:87-89`).

## Prerequisites (maintainer-provided, outside the code chunks)

These are **not implementation chunks** — the maintainer builds and installs the portlibs; the code chunks below assume `pkg-config` resolves the library in each target environment.

- **P1 — `switch-libsidplayfp`.** Build libsidplayfp for the Switch (devkitA64 via `switchvars.sh`, or a `dkp-makepkg` PKGBUILD), ReSIDfp only, hardware backends (exSID/HardSID) disabled, no external ROM dependency (built-in defaults). Install into the Switch portlibs prefix. Done when `pkg-config --exists libsidplayfp` resolves in the Switch env. (Desktop already has libsidplayfp.)
- **P2 — `sc68` for desktop *and* Switch.** Build the sc68 stack (file68 + unice68 + libsc68) for both the desktop dev machine (currently absent) and the Switch portlibs prefix. Done when `pkg-config --exists <sc68 .pc name>` resolves in both envs.

Chunk 9a needs no prerequisite. 9c is gated on P1. 9e is gated on P2.

## Architecture

Per plugin, the same five-point replication of `OpenMptPlugin` — nothing new in the engine:

```
FileSystem ──path──▶ PlayerController.findPluginFor(ext)  ── first plugin whose
                                                             getSupportedExtensions() matches
                          │
                          ▼
                 XxxPlugin : PlayerPlugin
   open(): build decoder (try/catch, no throw) ── cache XxxMetadata
   decode(): render 16-bit ─▶ convert to float32 stereo ─▶ buffer
   getMetadata(): return cached variant ──▶ Gui std::visit ──▶ drawXxxMetadata()
```

- **`XxxPlugin.{h,cpp}` (player domain).** GPL header, `final`, non-copyable, library type forward-declared to keep its header out of `PlayerPlugin.h` (`namespace ...; class ...;` + `std::unique_ptr` member). `create(sampleRate)` stashes the rate and fills `m_extensions` (lowercase, no dot). `open()` builds the decoder in `try/catch`, returns `false` on failure (**never throws**), applies cached settings, and caches the metadata struct. `close()` resets the decoder and sets `m_metadata = std::monostate{}`. `decode()` renders 16-bit into a scratch buffer and converts to float. `getName()` returns a unique pkg-config-style string (the settings key). `getPosition()/getDuration()/applySetting()` run under `m_mutex`; `getMetadata()` returns the cached value only.

- **`Metadata.h`** — one struct per family added as a variant alternative (fields matching TODO_5):
  ```cpp
  struct GmeMetadata  { std::string game, system, author, copyright, comment; int trackCount; };
  struct SidMetadata  { std::string title, author, released, sidModel, clock; };
  struct Sc68Metadata { std::string title, author, composer, hardware, ripper; };
  using TrackMetadata = std::variant<std::monostate, ModuleMetadata,
                                     GmeMetadata, SidMetadata, Sc68Metadata>;
  ```

- **`Gui.{h,cpp}`** — a `drawXxxMetadata(const XxxMetadata&)` declaration + definition (template: `drawModuleMetadata`) and one `std::visit` overload per struct.

- **`PlayerController::create()`** — `#include "plugins/XxxPlugin.h"` + one `m_plugins.emplace_back(std::make_unique<XxxPlugin>());`. New extensions don't overlap libopenmpt's, so registration order is not sensitive.

- **Multi-subtune formats.** NSF/SPC/GBS (GME), SID, and SNDH (sc68) hold multiple subtunes per file; the `PlayerPlugin` interface has no subtune concept. **Default to the file's default/first subtune** for now — per-subtune selection is a deliberate future extension (would touch the interface + UI), noted in Notes, not built here.

- **Settings (optional, TODO_6 pattern).** Each plugin may expose tunables via `getSettings()`/`applySetting()` (e.g. GME stereo/echo depth; SID model 6581/8580 + clock PAL/NTSC as `EnumOptions`). Ship minimal first; tunables can be added later without touching the engine.

## Task chunks (implement, verify, and commit one at a time)

- [x] **9a — GmePlugin** (no prerequisite; both platforms today). `src/player/plugins/GmePlugin.{h,cpp}` over libgme (`gme_open_data`/`gme_start_track`/`gme_play`/`gme_track_info`, forward-declare `struct Music_Emu`), extensions `ay, gbs, gym, hes, kss, nsf, nsfe, sap, spc, vgm, vgz`, default subtune 0. `GmeMetadata` (from `gme_info_t`: game/system/author/copyright/comment + `gme_track_count`) + `drawGmeMetadata`. Registration + **uncomment the libgme CMake stanza** and add `GmePlugin.cpp` to `add_executable`. **Scope added during work:** the whole player→SDL audio path was converted from float32 to native signed-16-bit PCM (`PlayerPlugin::decode(std::int16_t*, frames)`, `AUDIO_S16SYS`, libopenmpt int16 render overload, `AudioTap` stores int16 and converts to float only on read for the visualizer) — so decoders that emit 16-bit natively (GME/SID/sc68) need no conversion. Added GME settings (`stereo_depth` `IntRange`, `accuracy` `EnumOptions`); extracted shared `metadataTextRow`/`metadataCountRow`/`metadataTextBlock` Gui helpers for the coming SID/sc68 draw functions; tuned `BarsVisualizer` GAIN 1.4→1.15 for hotter GME content.
- [x] **9c — SidPlugin** (gated on prerequisite **P1**). `src/player/plugins/SidPlugin.{h,cpp}` over libsidplayfp (`sidplayfp` engine + `SidTune` + `ReSIDfpBuilder`, forward-declared), 16-bit→float, extensions `sid, psid, rsid`, default subtune = tune's start song. `SidMetadata` (from `SidTuneInfo`: title/author/released + SID model + clock) + `drawSidMetadata`. Registration + `pkg_check_modules(libsidplayfp …)` stanza + `add_executable` entry. Use built-in ReSIDfp defaults — **no external C64 ROM files**.
- [ ] **9e — Sc68Plugin** (gated on prerequisite **P2**). `src/player/plugins/Sc68Plugin.{h,cpp}` over libsc68 (`sc68_create`/`sc68_load`/`sc68_process`, forward-declared), 16-bit→float, extensions `sc68, sndh, snd`, default subtune = disc default track. `Sc68Metadata` (title/author/composer/hardware/ripper) + `drawSc68Metadata`. Registration + `pkg_check_modules(sc68 …)` stanza + `add_executable` entry.

Each chunk ends with green desktop + Switch builds, a docs update if classes/methods changed, user verification (a representative file actually plays and the Metadata tab renders the new struct), then a commit. Run cpp-reviewer on the diff before committing; run the pending Switch hardware test before merging the branch.

## Files to change

1. **`src/player/plugins/GmePlugin.{h,cpp}`**, **`SidPlugin.{h,cpp}`**, **`Sc68Plugin.{h,cpp}`** (new) — the three `PlayerPlugin` implementations.
2. **`src/player/Metadata.h`** — `GmeMetadata` / `SidMetadata` / `Sc68Metadata` structs + variant alternatives.
3. **`src/gui/Gui.{h,cpp}`** — `drawGmeMetadata` / `drawSidMetadata` / `drawSc68Metadata` decls + defs + `std::visit` overloads.
4. **`src/player/PlayerController.cpp`** — three `#include`s + three `emplace_back` lines in `create()`.
5. **CMakeLists.txt** — three `.cpp` files added to `add_executable`; three `pkg_check_modules(... REQUIRED IMPORTED_TARGET ...)` + `target_link_libraries(... PkgConfig::...)` stanzas (single stanza each, both platforms; uncomment the libgme stub).

## Docs

- **`docs/audio.md`** — add the three plugins to the `PlayerPlugin` class diagram + notes; document the 16-bit→float conversion contract and the default-subtune behavior for multi-subtune formats.
- **`docs/ui.md`** — the three new `drawXxxMetadata` methods on `Gui`.
- **`docs/build-portlibs.md`** (new) — record exactly how `switch-libsidplayfp` and the `sc68` stack were built/installed for the Switch (and sc68 for desktop), so the builds are reproducible.

## Coordination

- **Requires TODO_5** (metadata variant + exhaustive visit) and **TODO_6** (plugin settings) for the metadata/settings paths; **TODO_1** for the engine.
- **TODO_7** (remote FTP source) works with every new plugin for free — remote files route through the same `findPluginFor`.
- Prerequisites P1/P2 (portlib builds) are maintainer-provided and block chunks 9c/9e respectively; 9a is independent.

## Order & verification

Per chunk: green **desktop** (`cmake --build cmake-build-debug`) **and Switch** (`cmake-build-switch`, per CLAUDE.md); run `./cmake-build-debug/OSP2` from the repo root.

1. **9a** — play an `.nsf`, `.spc`, and `.vgm`: audio is correct, the Metadata tab shows game/system/author, position/duration advance; Switch build compiles and links libgme.
2. **9c** — play a `.sid`: audio is correct (built-in ReSIDfp, no ROMs), Metadata shows title/author/released/model; Switch build links libsidplayfp.
3. **9e** — play an `.sndh`/`.sc68`: audio is correct, Metadata shows title/author/hardware; Switch build links libsc68.

## Notes

- **Per-subtune selection** (choosing among the many songs in an NSF/SID/SNDH) is out of scope: it would extend the `PlayerPlugin` interface (subtune count + select) and the UI. Default subtune only for now.
- All three render at `PlayerController::SAMPLE_RATE` (48 kHz) — GME resamples internally; SID/sc68 take the rate at init. No resampler needed.
- Keep each library's header out of `PlayerPlugin.h` via forward declaration, exactly as `OpenMptPlugin` does with `namespace openmpt { class module; }`.
