# TODO_30 — SID durations from the HVSC Songlengths database

> SID tunes carry **no** intrinsic length — libsidplayfp cannot report one, so `SidPlugin::getDuration()` returns `0` (open-ended) for every tune. The community-standard source of truth is **HVSC's Songlengths database** (`Songlengths.md5`), which maps each tune's MD5 to per-subtune play times. Compute the loaded tune's MD5, look it up, and report the real length. Requires TODO_9 (SidPlugin). Independent of TODO_29. Larger than a one-liner because it introduces an external data file.

## Context

`SidPlugin` (`src/player/plugins/SidPlugin.cpp`, see [audio.md](../audio.md)) drives libsidplayfp and, per its docstring, *"SID tunes loop indefinitely and carry no intrinsic length, so `getDuration()` returns `0` (unknown) and playback never self-ends."* The fix is purely additive metadata — it does **not** change decode/loop behaviour (a SID with a known length can still loop; the length is display + a candidate auto-advance point later).

The HVSC **Songlengths database** (`DOCUMENTS/Songlengths.md5`, freely redistributable, updated per HVSC release) is an INI-like file:

```
[Database]
<32-hex-md5>=M:SS M:SS ...     ; one time token per subtune, in order
```

Since HVSC #68 the keys use the **"new" MD5** (`SidTune::createMD5New()` in libsidplayfp); the older `createMD5()` matched the pre-#68 format. Use `createMD5New()` to match a current DB.

## Design decisions (resolve before implementing)

1. **Where the DB comes from — bundle vs. download.**
   - *Bundle in `romfs/`*: `Songlengths.md5` is a few MB, works offline, but bloats the `.nro` and pins a fixed HVSC version.
   - *Download-on-first-use + cache* (recommended): fetch once from a configured URL into the app cache dir (same cache root the FTP source uses), parse from there; ship without it. Keeps the `.nro` small and lets the DB update. Needs a source URL and graceful "not downloaded yet → open-ended, as today" fallback.
   - A hybrid (optional bundled seed, cache overrides) is possible but adds complexity; pick one.
2. **Parse + index strategy.** The DB has ~tens of thousands of entries. Load it once (lazily, off the audio thread) into a `std::unordered_map<md5, std::vector<seconds>>` owned outside the audio path; `getDuration()` reads only the cached `m_duration` for the current tune (same lock discipline as the other cached getters — no lookup on the audio thread).
3. **Which subtune.** `SidPlugin` currently plays the **default subtune only** (`selectSong(0)`). Report that subtune's length now; when per-subtune selection lands, index by the selected subtune.

## The fix (sketch)

- Add a small **SongLengthDb** helper (own file under `src/player/` or a `sidlength/` folder): load + parse `Songlengths.md5`, expose `lookup(md5, subtune) -> std::optional<double>`.
- In `SidPlugin::open`, after the tune loads, compute `createMD5New()`, look up the default subtune's length, and cache it in `m_duration` (`0` when absent → unchanged open-ended behaviour).
- Wire the DB source per the decision above (bundle path in `romfs/`, or a download+cache step with a config-provided URL).
- Update `SidPlugin::getDuration()`'s contract: real length when the DB knows the tune, `0` otherwise.

## Files to change

1. **`src/player/plugins/SidPlugin.cpp` / `.h`** — md5 compute + `m_duration` from the DB in `open()`.
2. **new `src/player/SongLengthDb.{h,cpp}`** (or similar) — DB load/parse/lookup; add to `CMakeLists.txt`.
3. **Distribution glue** — `romfs/` asset **or** a cache download step (per decision 1), plus config for the source/URL if downloaded.

## Docs

- **`docs/audio.md`** — update the SidPlugin note: `getDuration()` now returns the HVSC Songlengths time (via `createMD5New()` lookup) when available, `0` otherwise; describe where the DB lives and the off-audio-thread parse.
- **`THIRD_PARTY_NOTICES.md`** — attribute the HVSC Songlengths database (author/licence/redistribution terms).
- **`docs/settings.md`** — if a download URL is user-configurable.

## Coordination

- Requires **TODO_9** (SidPlugin). Independent of **TODO_29** (GME m3u). If per-subtune SID selection is ever added, revisit the "which subtune" lookup.

## Verification

- Desktop + Switch builds green.
- A SID present in the Songlengths DB shows its real default-subtune length in the player bar; the same tune with the DB absent/undownloaded stays open-ended (no regression).
- A tune not in the DB stays open-ended.
- No DB lookup or file I/O happens on the audio thread (durations are cached at `open()`).
- MOD / NSF / sc68 durations are unchanged.
