# TODO_21 — Accurate NES/NSF duration (drop the fake 2:30)

> NES (`.nsf`) tracks always show a **2:30** duration — the same value for every subsong — because libgme returns its built-in default track length for files that carry no real length metadata. Read the real length when the file actually provides one; otherwise show open-ended elapsed time instead of a fake duration. Single chunk. Requires TODO_9 (GmePlugin) + TODO_18 (subtracks).

## Context

`GmePlugin::startTrack` (`src/player/plugins/GmePlugin.cpp:148`) computes the reported duration from a single field:

```cpp
m_duration = info->play_length > 0 ? info->play_length / 1000.0 : 0.0;
```

`info` comes from `gme_track_info(m_emu.get(), &info, index)` (`GmePlugin.cpp:126`). libgme's `gme_info_t` exposes `length`, `intro_length`, `loop_length`, **and** `play_length`. A bare `.nsf` header carries **no** per-track length (only NSFe length tags or an accompanying `.m3u` do), so libgme fills `play_length` with its built-in default = **150000 ms = 2:30** for every subsong. The current code reads only `play_length`, so it faithfully reports that default — hence the constant 2:30. This is a libgme limitation, not an OSP2 bug; the fix is to stop trusting the default and only show a length we actually know.

Duration flows: `GmePlugin::getDuration()` (`GmePlugin.cpp:183`) → `PlaybackStatus.durationSeconds` (`PlayerController.cpp:288`, forced to `0.0` when STOPPED) → player bar (`Gui::drawPlayerBar`, `Gui.cpp:770-815`). The progress bar already tolerates a zero duration (`fraction` stays `0` when `durationSeconds <= 0`, `Gui.cpp:773`), so an "unknown duration" state is mostly a labelling concern.

## The fix (single chunk)

- In `GmePlugin::startTrack`, derive `m_duration` from the **real** length only:
  - use `info->length / 1000.0` when `info->length > 0` (explicit track length from NSFe / `.m3u`);
  - else use `(info->intro_length + info->loop_length) / 1000.0` when those are present/positive;
  - else `0.0` (**unknown** — do *not* fall back to `play_length`'s default guess).
- Player-bar label (`Gui.cpp:770-815`): when `durationSeconds <= 0`, render elapsed-only (open-ended) — e.g. `1:23` or `1:23 · --:--` — instead of `pos / 2:30`. Keep the current `pos / total` rendering when a real duration exists.
- Leave the loop interaction intact (`gme_set_autoload_playback_limit`, `GmePlugin.cpp:115`): with loop on the track never terminates on length anyway.

## Files to change

1. **`src/player/plugins/GmePlugin.cpp`** — duration derivation in `startTrack` (`:148`).
2. **`src/gui/Gui.cpp`** — player-bar duration label for the unknown-duration case (`drawPlayerBar`, `:770-815`).

## Docs

- **`docs/audio.md`** — note that GME duration comes from `gme_info_t.length` (real NSFe/`.m3u` length) and is *unknown* (open-ended) for bare NSF; the default `play_length` is deliberately ignored.

## Coordination

- Requires **TODO_9** (GmePlugin) and **TODO_18** (subtracks). Independent of the other TODO_21+ items; touches `Gui.cpp` so serialise with them.

## Verification

- Desktop + Switch builds green.
- A bare NSF (no NSFe length, no `.m3u`) shows open-ended playback — no 2:30 and no bogus progress fill.
- An NSFe / `.m3u`-accompanied file with real per-track times shows those durations, and they differ per subsong.
- SID / MOD / sc68 durations are unchanged.
