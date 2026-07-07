# Audio domain

Playback core in `src/player/`. `PlayerController` is the SDL boundary (audio device, threading); `PlayerPlugin` implementations wrap one decoder library each and are SDL-free.

```mermaid
classDiagram
    class PlayerState {
        <<enumeration>>
        STOPPED
        PLAYING
        PAUSED
    }

    class PlayResult {
        <<enumeration>>
        Ok
        Unsupported
        DecodeError
    }

    class PlayerController {
        +int SAMPLE_RATE$ = 48000
        +int CHANNELS$ = 2
        +int BUFFER_FRAMES$ = 1024
        -vector~unique_ptr~PlayerPlugin~~ m_plugins
        -SDL_AudioDeviceID m_device
        -mutex m_mutex
        -PlayerPlugin* m_activePlugin
        -PlayerState m_state
        -path m_currentPath
        -atomic_bool m_trackEnded
        -thread m_loadWorker
        -atomic_bool m_loading
        -bool m_loadPending
        -bool m_loadCancelled
        -PlayerPlugin* m_loadPlugin
        -path m_loadPath
        -bool m_loadSucceeded
        -optional~PlayResult~ m_playResult
        -AudioTap m_audioTap
        +create()
        +destroy()
        +play(path)
        +play()
        +pause()
        +stop()
        +update()
        +getState() PlayerState
        +getCurrentFileName() string
        +getCurrentPath() path
        +getCurrentTitle() string
        +getSubtrackCount() int
        +getCurrentSubtrack() int
        +getStatus() PlaybackStatus
        +isSupported(path) bool
        +consumeTrackEnded() bool
        +isLoading() bool
        +consumePlayResult() optional~PlayResult~
        +cancelLoad()
        +readLatestAudio(out, maxFrames) size_t
        +applyPluginSetting(pluginName, key, value)
        +selectSubtrack(index)
        +getPluginSettings() vector~pair~string, vector~PluginSetting~~~
        -audioCallback(userdata, stream, len)$
        -decode(stream, len)
        -findPluginFor(path) PlayerPlugin*
        -loadTrack(plugin, path)
    }

    class AudioTap {
        +size_t MAX_FRAMES$ = 1024
        +size_t CHANNELS$ = 2
        -atomic~uint32_t~ m_seq
        -int16_t[] m_samples
        -size_t m_frameCount
        +publish(frames, frameCount)
        +read(out, maxFrames) size_t
    }

    class PlayerPlugin {
        <<abstract>>
        +create(sampleRate)*
        +destroy()*
        +open(path)* bool
        +close()*
        +decode(buffer, frames)* int
        +getName()* string
        +getSupportedExtensions()* vector~string~
        +getTitle()* string
        +getPosition()* double
        +getDuration()* double
        +getMetadata()* TrackMetadata
        +getSettings() vector~PluginSetting~
        +applySetting(key, value)
        +getSubtrackCount() int
        +getCurrentSubtrack() int
        +selectSubtrack(index)
    }

    class OpenMptPlugin {
        -int m_sampleRate
        -vector~string~ m_extensions
        -unique_ptr~openmpt_module~ m_module
        -TrackMetadata m_metadata
        -int m_stereoSeparation
        -int m_interpolation
        -int m_loop
    }

    class GmePlugin {
        -int m_sampleRate
        -vector~string~ m_extensions
        -unique_ptr~Music_Emu~ m_emu
        -TrackMetadata m_metadata
        -string m_title
        -double m_duration
        -int m_trackCount
        -int m_currentTrack
        -int m_stereoDepth
        -int m_accuracy
        -int m_loop
        -startTrack(index) bool
    }

    class SidPlugin {
        -int m_sampleRate
        -vector~string~ m_extensions
        -unique_ptr~sidplayfp~ m_engine
        -unique_ptr~ReSIDfpBuilder~ m_builder
        -unique_ptr~SidTune~ m_tune
        -vector~int16_t~ m_mixBuffer
        -size_t m_mixPos
        -size_t m_mixFrames
        -TrackMetadata m_metadata
        -string m_title
        -int m_model
        -int m_clock
    }

    class Sc68Plugin {
        -int m_sampleRate
        -vector~string~ m_extensions
        -sc68_t* m_sc68
        -TrackMetadata m_metadata
        -string m_title
        -double m_duration
        -uint64_t m_playedFrames
        -bool m_ended
        -int m_asid
        -int m_loop
    }

    class PluginSetting {
        <<value object>>
        +string key
        +string label
        +variant~IntRange, EnumOptions~ shape
        +int value
        +bool appliesOnNextTrack
    }

    class IntRange {
        <<value object>>
        +int min
        +int max
    }

    class EnumOptions {
        <<value object>>
        +vector~string~ labels
    }

    class PlaybackStatus {
        <<value object>>
        +PlayerState state
        +string title
        +string fileName
        +double positionSeconds
        +double durationSeconds
        +int subtrackCount
        +int currentSubtrack
    }

    class ModuleMetadata {
        <<value object>>
        +string title
        +string artist
        +string format
        +string tracker
        +int channels
        +int patterns
        +int samples
        +int instruments
        +string message
    }

    class GmeMetadata {
        <<value object>>
        +string game
        +string system
        +string author
        +string copyright
        +string comment
        +int trackCount
    }

    class SidMetadata {
        <<value object>>
        +string title
        +string author
        +string released
        +string sidModel
        +string clock
    }

    class Sc68Metadata {
        <<value object>>
        +string title
        +string author
        +string composer
        +string hardware
        +string ripper
    }

    class TrackMetadata {
        <<variant>>
        monostate | ModuleMetadata | GmeMetadata | SidMetadata | Sc68Metadata
    }

    PlayerController --> PlayerState : state machine
    PlayerController ..> PlayResult : consumePlayResult() outcome
    PlayerController ..> PlaybackStatus : getStatus() snapshot
    PlayerController ..> TrackMetadata : getMetadata() cached value
    PlayerController "1" o-- "*" PlayerPlugin : owns, dispatches by extension
    PlayerController "1" *-- "1" AudioTap : lock-free tap
    PlayerPlugin <|-- OpenMptPlugin : libopenmpt (mod, xm, s3m, it, ...)
    PlayerPlugin <|-- GmePlugin : libgme (nsf, spc, vgm, gbs, ...)
    PlayerPlugin <|-- SidPlugin : libsidplayfp (sid, psid, rsid)
    PlayerPlugin <|-- Sc68Plugin : libsc68 (sc68, sndh, snd)
    TrackMetadata ..> ModuleMetadata : alternative (libopenmpt)
    TrackMetadata ..> GmeMetadata : alternative (libgme)
    TrackMetadata ..> SidMetadata : alternative (libsidplayfp)
    TrackMetadata ..> Sc68Metadata : alternative (libsc68)
    PlayerPlugin ..> PluginSetting : publishes descriptors
    PluginSetting ..> IntRange : shape alternative
    PluginSetting ..> EnumOptions : shape alternative
```

## Threading

- The SDL audio thread pulls samples via `audioCallback` → `decode()` (48 kHz, signed-16-bit interleaved stereo, `AUDIO_S16SYS`). Plugins render native int16 (`PlayerPlugin::decode(std::int16_t*, frames)`) — no format conversion on the audio path (libgme/libsidplayfp/libsc68 emit 16-bit natively; libopenmpt has a native int16 render overload). The **only** int16→float conversion is in `AudioTap::read`, off the audio thread, for the visualization layer.
- `m_mutex` guards `m_state` and `m_activePlugin` (including the decoder inside it); locked by the callback and by play/pause/stop/getters.
- `getStatus()` returns a `PlaybackStatus` (`src/player/PlaybackStatus.h`) snapshot — state, title, filename, position, duration — built under a **single** `m_mutex` lock (it inlines the reads rather than calling the re-locking getters, since `m_mutex` is non-recursive). The plugin virtuals `getPosition()`/`getDuration()` (seconds) read the shared decoder, so they are contractually **only** called under `m_mutex`. Position/duration are `0` when stopped or unknown.
- Pause is controller state only — the device runs for the whole app lifetime and the callback emits silence when not PLAYING.
- End of track: the audio thread flips state to STOPPED and sets `m_trackEnded` (atomic); the main loop consumes it once per frame (`consumeTrackEnded()`) to auto-advance. Track teardown (`close()`) never happens on the audio thread.
- **Async decode (loading off the UI thread).** `plugin->open()` parses a whole module, which can be slow for a large file, so it runs on a **player-owned worker** (`m_loadWorker`) rather than the UI thread — the UI stays responsive and the existing browser overlay covers the load (see [ui.md](ui.md) / [application.md](application.md)). `play(path)` is now `void` and asynchronous: it resolves the plugin (`findPluginFor`; a null match is a defensive no-op since callers gate on `isSupported`), then under `m_mutex` closes the current plugin, nulls `m_activePlugin`, sets `STOPPED`, clears `m_currentPath` and `m_trackEnded` — this makes the audio thread output silence and **guarantees the plugin being opened is never the one the audio thread decodes**. It then joins any still-running previous load (a brief block only if a transport press interrupts an in-flight load — a browser click can't, the overlay disables the browser during a load) and launches `loadTrack(plugin, path)`. `loadTrack` (worker) calls `plugin->open()` **off `m_mutex`** (safe: the plugin is inactive), stores the outcome in `m_loadSucceeded` under `m_mutex`, then clears the `m_loading` atomic **last** so `update()` only observes a fully-published result. `update()` (main thread, each frame) reaps a finished load: it joins the worker (a happens-before for `m_loadSucceeded`) and, under `m_mutex`, on success swaps the freshly-opened plugin in (`m_activePlugin = m_loadPlugin`, `PLAYING`, `m_currentPath`) or on failure `close()`s it (a failed `open()` may leave a half-parsed module), then publishes the outcome in `m_playResult`. `isLoading()` tracks the main-thread `m_loadPending` (not the atomic) so the overlay holds until the swap-in frame with no one-frame flicker; `consumePlayResult()` hands the outcome to `Application` once for its auto-advance-skip / error decision. **The outcome is a `PlayResult`** (`src/player/PlayResult.h`), a three-way enum richer than the old `bool`: `Unsupported` (no plugin matched the extension — set **synchronously** in `play()` before it early-returns, the only outcome not produced by the worker), `Ok` (the module opened and playback started) and `DecodeError` (a matching plugin's `open()` failed to parse) — the last two published by `update()` when it reaps the worker (`m_playResult = succeeded ? Ok : DecodeError`). `Application` maps each to a user-facing message for a direct click, or a silent skip when auto-advancing (see [application.md](application.md)). `cancelLoad()` sets `m_loadCancelled`: the parse **cannot be interrupted** (it finishes in the background), but its result is dropped — `update()` closes the opened plugin, starts no playback and produces **no** `m_playResult` (so a cancel never triggers auto-advance or an error popup). `stop()` applies the same drop to any in-flight load (the transport bar stays live while the browser overlay is up), so a Stop pressed mid-load is honoured instead of being overridden by the load swapping a plugin in when it finishes. `destroy()` **joins `m_loadWorker` before** closing the device / destroying plugins, so no parse is ever in flight during teardown.
- **Audio tap (visualization).** `m_audioTap` (`src/player/AudioTap.h`) is a single-producer / single-consumer **seqlock** publishing the most recently decoded block of interleaved stereo **int16** frames, **independent of `m_mutex`**. `read()` converts int16→float (normalized) for the visualizer, which is why `readLatestAudio()` still hands out `float`. Producer: the audio thread calls `publish()` inside `decode()` after a successful decode (only the real `frames_written` frames, before the end-of-track zero-padding rewrites the buffer tail) — it never blocks and never allocates. Consumer: the main thread calls `readLatestAudio()` (→ `AudioTap::read()`), which spins only on a torn read and **never touches `m_mutex`**. So the audio thread never blocks to publish and the reader never contends the decode lock. When not PLAYING, `decode()` publishes nothing, so the reader sees a stale block; the visualization layer passes `frameCount = 0` and decays the visual to rest.
- `destroy()` closes the audio device before destroying plugins.
- **Plugin settings follow the same lock discipline as decode.** A plugin publishes tunables as `PluginSetting` descriptors (`src/player/PluginSetting.h`): a `key`/`label`, a current `int value`, a `shape` variant — `IntRange` (→ slider) or `EnumOptions` (→ combo, `value` = index into `labels`) — and an `appliesOnNextTrack` flag (default `false`) that, when set, makes the settings popup render an "Applies on the next track" hint under the widget for *deferred* settings that only take effect when the next track starts (as opposed to live ones applied immediately in `applySetting`). `getSettings()` returns plain **cached** members (no decoder access), while `applySetting(key, value)` may touch the *live* decoder, so it is called **only** through `PlayerController::applyPluginSetting(pluginName, key, value)`, which takes `m_mutex` (plugins are matched by `getName()`; no match = no-op). `getPluginSettings()` also locks and returns `(pluginName, descriptors)` per plugin for the UI. `OpenMptPlugin` exposes `stereo_separation` (`IntRange 0–200`, default 100 → `RENDER_STEREOSEPARATION_PERCENT`), `interpolation` (`EnumOptions` Default/None/Linear/Cubic/Sinc → filter lengths 0/1/2/4/8 → `RENDER_INTERPOLATIONFILTER_LENGTH`), and `loop` (`EnumOptions` Off/On, default Off → `set_repeat_count`, `On` → `-1` so the module loops forever and never signals end-of-track — a seamless loop handled entirely by the decoder, no app-level re-play); all three are cached members, applied to the module in `open()` and immediately in `applySetting()` — OpenMpt's `loop` is therefore **live** (`set_repeat_count` retimes the current playthrough), unlike the GME and sc68 loop settings which are deferred to the next track (`appliesOnNextTrack`, see below). **Values are clamped/normalized on store** (separation to 0–200, interpolation to a valid index) so a hand-edited INI can never feed `set_render_param` an out-of-range value — that call throws, and an unchecked value would otherwise make every subsequent `open()` fail. Persisted `[plugin.<name>]` values are pushed once at startup by `main.cpp` via `applyPluginSetting` (see [settings.md](settings.md)).
- **Metadata is transcoded to UTF-8 at the plugin boundary.** Dear ImGui only renders UTF-8, but the decoder libraries hand OSP2 metadata in each format's legacy charset, so every captured string is transcoded during `open()`, where the source charset is known, via the shared free function `toUtf8(std::string_view bytes, Charset)` (`src/player/Charset.{h,cpp}`). Charsets: **GME = Shift-JIS** (NSF/GBS/… header fields; ASCII tags — SPC/VGM/… — pass through unchanged since Shift-JIS is ASCII-compatible), **SID = Latin-1** (PSID/STIL info strings), **SC68 = Latin-1** (Atari ST / Amiga tag text; the library-generated hardware strings are ASCII and unaffected). **OpenMPT is left untouched** — libopenmpt already returns UTF-8. Only decoder-derived strings are transcoded: a title that falls back to `path.filename()` is already UTF-8 from the filesystem and must **not** be run through `toUtf8`. `toUtf8` is dependency-free and Switch-portable: Latin-1 is a per-byte expansion, Shift-JIS is a decoder loop (ASCII passthrough, halfwidth-katakana range `0xA1–0xDF`, and double-byte lead bytes looked up by `std::lower_bound` in a **vendored, generated** table). That table is `src/player/ShiftJisTable.inc` — sorted `{uint16 sjis, uint16 codepoint}` pairs for double-byte sequences only, generated from Python's `shift_jis` codec by `scripts/gen_sjis.py` (regenerate: `python3 scripts/gen_sjis.py`). Malformed/truncated input never crashes and never reads out of bounds: undecodable bytes become U+FFFD. (`Charset`/`toUtf8` is a free helper, not a class, so it is intentionally absent from the class diagram above. Note: rendering CJK glyphs also needs a CJK font in the atlas — TODO_20b.)
- **Metadata is captured once, not read per frame.** Each library exposes a different metadata shape, so it travels as `TrackMetadata` (`src/player/Metadata.h`) — a `std::variant<std::monostate, ModuleMetadata, ...>`, one struct per plugin family, `monostate` = no track. Reading the decoder every frame would contend with `decode()`, so a plugin's `getMetadata()` returns a value **cached during `open()`** and cleared to `monostate` in `close()`; it must never touch the decoder object. `PlayerController::getMetadata()` still locks `m_mutex` (to read `m_activePlugin` safely) and returns the plugin's cached value, or a default (`monostate`) when nothing is active. `Application` refetches only on track change (see [application.md](application.md)), so the lock is taken rarely, not per frame.

- **Subtracks follow the same lock discipline as settings.** Many chiptune formats pack several tunes into one file (GME especially — NSF/GBS/VGM/KSS/…). `PlayerPlugin` exposes three virtuals with single-track defaults so non-multi-track plugins need no change: `getSubtrackCount()` (default `1`) and `getCurrentSubtrack()` (default `0`) are **cached** reads (no decoder access, like `getMetadata`), while `selectSubtrack(int)` (default no-op) may touch the *live* decoder, so it is called **only** through `PlayerController::selectSubtrack(index)`, which takes `m_mutex` (same contract as `applyPluginSetting`) and also **clears `m_trackEnded`** — a manual subtrack change must not be clobbered by auto-advance (consistent with `play()`/`stop()`). `getStatus()` surfaces the two counts through `PlaybackStatus` (`subtrackCount`/`currentSubtrack`; `1`/`0` when no plugin is active), which rides in `UiState.status`. Two lightweight locked getters — `getSubtrackCount()` (→ `1` when idle) and `getCurrentSubtrack()` (→ `0` when idle) — expose the same two values individually as the navigation-query API, distinct from the full `getStatus()` snapshot; `Application::advance` reads them to decide subtrack-step vs. file-advance (NEXT/PREVIOUS and auto-advance both route through it, see [application.md](application.md)). **GmePlugin** overrides all three: `open()` sets `m_trackCount = gme_track_count(...)` then calls the private `startTrack(0)`; `selectSubtrack(index)` clamps to `[0, count)` and calls `startTrack(clamped)`, leaving the current track (and **not** resetting `m_emu`) on failure since the file is still valid. `startTrack(index)` is the shared per-track init: `gme_start_track(index)`, **re-apply** the cached stereo depth (a start_track can reset per-track effects), `gme_track_info(index)`, then rebuild `m_metadata`/`m_title`/`m_duration` (game/system/author/copyright/comment are file-level and identical across subtracks; song and length are per-track) and set `m_currentTrack`. **Duration is a *real* length only:** `m_duration` is taken from `gme_info_t.length` (explicit NSFe/`.m3u` time), else `intro_length + loop_length`, else `0` = **unknown**. libgme's `play_length` is deliberately **ignored** — a bare NSF header carries no per-track length, so libgme fills it with its built-in default (150000 ms = 2:30) for every subsong; reading it would fake a constant 2:30. An unknown (`0`) duration renders open-ended in the player bar (`--:--`, no progress fill) rather than a bogus time. **Where that real length comes from:** a bare header rarely carries one, but a companion **`.m3u`** playlist (the "NSF M3U" convention) does. `open()`, right after `gme_open_data`, overlays one via the private `overlaySiblingM3u(tunePath)` (a **local-only**, strictly best-effort step — a missing/malformed playlist is logged and ignored, never a reason to fail the open) which handles both real-world layouts: (1) a **combined** `<stem>.m3u` next to the tune → `gme_load_m3u`; else (2) the **exploded** layout (common Zophar dumps) — several per-track `.m3u` files in the tune's directory, each naming one subtrack and referencing the tune by filename (`TUNE.gbs::GBS,<track>,<title>,<time>,…`) — whose referencing lines are gathered (skipping other tunes' lines), ordered by their source `.m3u` filename to recover album order, concatenated, and handed to `gme_load_m3u_data`. Either way libgme then reports the **curated** playlist as the subtrack list, with `gme_info_t.song`/`length` populated per entry — so `gme_track_count` (read on the next line, driving `m_trackCount`) and the `startTrack` caches pick up the curated titles and real durations automatically. So a live subtrack switch refreshes exactly the caches `getTitle()`/`getDuration()`/`getMetadata()` read, under the lock. `GmeMetadata::trackCount` stays populated as before (display-only); `m_trackCount` is the navigation source of truth.

## Adding a decoder plugin

One class under `src/player/plugins/` implementing `PlayerPlugin`, one `emplace_back` in `PlayerController::create()` (registration order = dispatch priority on extension overlap), one CMake stanza. Shipped: libopenmpt (`OpenMptPlugin`), libgme (`GmePlugin` — `ay, gbs, gym, hes, kss, nsf, nsfe, sap, spc, vgm, vgz`; renders libgme's interleaved-stereo 16-bit straight into the output buffer, no conversion; exposes `stereo_depth` (`IntRange 0–100` → `gme_set_stereo_depth`), `accuracy` (`EnumOptions` Fast/Accurate → `gme_enable_accuracy`), and `loop` (`EnumOptions` Off/On, `appliesOnNextTrack` — **deferred**: consumed at the next `startTrack()`. **End-of-track for auto-advance:** most GME formats (NSF/GBS/KSS/…) never self-limit — only SPC obeys `gme_set_autoload_playback_limit` — so with `loop` **Off** `startTrack` calls `gme_set_fade_msecs` after `gme_start_track`, fading the track out at its length (or a `150000 ms` default when the length is unknown, e.g. a bare NSF; fade duration is `length/10` clamped to `500…8000 ms`) so it ends and playback advances to the next subtrack/file. With `loop` **On** no fade is set (and SPC's limit is disabled), so the tune repeats forever and never auto-advances. Without the explicit fade a non-SPC tune would loop indefinitely regardless of the setting)), libsidplayfp (`SidPlugin` — `sid, psid, rsid`; see below), libsc68 (`Sc68Plugin` — `sc68, sndh, snd`; see below). A plugin with no tunables need not override `getSettings()`/`applySetting()` (safe defaults: `{}` / no-op); one that does override them gets persistence and settings UI for free via the `PluginSetting` descriptor mechanism.

**SidPlugin (libsidplayfp v3).** Drives the `sidplayfp` engine with a `ReSIDfpBuilder` (the accurate ReSIDfp emulation; requires the `libresidfp` companion library on both platforms — see [build-portlibs.md](build-portlibs.md)). **Optional C64 ROMs:** `create()` loads `romfs/roms/{kernal-901227-03,basic-901226-01,chargen-901225-01}.bin` if present and hands them to `setRoms()` (each skipped on absence or size mismatch — `setRoms` copies the data, so the load buffers are transient). PSID tunes (the vast majority) play with **no** ROMs; RSID tunes that boot like a real C64 need at least the KERNAL. The ROMs are copyrighted Commodore firmware, so they are `.gitignore`d (not in the repo) and only baked into the `.nro` if the user supplies them. libsidplayfp's v3 API is a two-step `play(cycles)` → `mix(short*, samples)` that yields a **variable** number of interleaved-stereo 16-bit samples per call, so `decode()` runs the emulation in fixed cycle slices and buffers the surplus in `m_mixBuffer` (`m_mixPos`/`m_mixFrames`), draining it before emulating more — the scratch is sized once in `open()` (`getBufSize`) so the audio thread never allocates. SID tunes loop indefinitely and carry no intrinsic length, so `getDuration()` returns `0` (unknown) and playback never self-ends — SID loops inherently, so it exposes **no** `loop` setting. **Default subtune only** (`selectSong(0)`); per-subtune selection is a deliberate future extension. Settings: `sid_model` (`EnumOptions` Auto/MOS 6581/MOS 8580) and `clock` (`EnumOptions` Auto/PAL/NTSC), both **applied on the next `open()`** (a live change would restart the tune) and clamped on store — both carry `appliesOnNextTrack` so the popup shows the deferred hint.

**Sc68Plugin (libsc68).** Wraps the `sc68_t` instance (Atari ST YM-2149/STE and Amiga Paula formats, plus SNDH archives). Needs no external ROMs. `create()` runs the refcounted global `sc68_init(0)`, creates the instance at the output sample rate, and forces `SC68_SET_PCM`/`SC68_PCM_S16` so `sc68_process()` writes interleaved signed-16-bit stereo straight into `decode()`'s buffer — **no conversion**. `open()` reads the whole file into memory and hands it to `sc68_load_mem()` (matching Sid/Gme; the read is wrapped so `open()` never throws), then `sc68_play(SC68_DEF_TRACK, m_loop ? SC68_INF_LOOP : SC68_DEF_LOOP)` starts the disk's **default track** (per-subtune selection is a future extension) with the deferred `loop` setting applied (see below). `decode()` runs a **fill loop** over `sc68_process()`: `sc68_play()` only *posts* the track change, which `sc68_process()` applies lazily on its first call — reporting `SC68_CHANGE` with **zero** frames before emulating a sample — so a single call can fall short without the track ending (a naive one-shot decode would misread that as immediate end-of-track and never play). The loop keeps calling until the buffer is full or the track truly ends, latches `m_ended` on `SC68_END` or `SC68_ERROR` (tested with `==`, since `SC68_ERROR == ~0` would otherwise match the normal status bits), and carries a small consecutive-empty-pass guard so a persistent `SC68_IDLE` can never spin the audio thread. Returning fewer frames than requested signals end-of-track to the controller; `getPosition()` is derived from an accumulated decoded-frame counter (sc68 has no cheap position query) while `getDuration()` is the real track length from `sc68_music_info` `time_ms` (track time, falling back to disk time). Metadata (`Sc68Metadata`) is copied out of the disk info during `open()` because those pointers are only valid until `sc68_close()`: title (falling back to album), author, composer (scanned from the track then disk tag arrays for a case-insensitive `composer` key), hardware name, and ripper. Exposes two settings, both cached members clamped on store, **deferred** to the next `open()` (`appliesOnNextTrack`) and shown with the popup hint. `asid` (`EnumOptions` Off/On/Force → `SC68_SET_ASID`, the aSIDifier that reshapes YM/ST output toward a SID-like waveform) is applied at the next `open()` — `sc68_play()` loads the asidifier replay, so a live change would need a track restart — matching Sid's model/clock pattern (only tracks advertising aSID caps are affected, a harmless no-op otherwise). `loop` (`EnumOptions` Off/On) is fixed by the `sc68_play()` loop-count argument (`On` → `SC68_INF_LOOP` for an infinite loop, `Off` → `SC68_DEF_LOOP`); `SC68_CUR_TRACK` is a getter, so the loop count can't change live and only takes effect on the next `open()`. **Switch note:** libsc68 references a plain `basename` symbol that devkitA64's newlib declares but does not provide, so the Switch build compiles `src/player/plugins/switch_compat.c` (a minimal C-linkage `basename`, deliberately including no libc string headers whose asm aliases would rename the symbol); the desktop libc already provides it.
