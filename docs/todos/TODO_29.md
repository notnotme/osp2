# TODO_29 — GME durations from a companion `.m3u` (local)

> TODO_21 made bare NSF/GBS/… tracks show open-ended time because libgme reports no real length. Many GME tunes ship with a companion **`.m3u`** that carries per-subtune titles *and* lengths (the "NSF M3U" convention: `tune.gbs::GBS,0,Title,2:30,,10`). libgme overlays that data via `gme_load_m3u` / `gme_load_m3u_data`, which populate the `gme_info_t.song`/`length` fields TODO_21 already reads — and it makes libgme expose the **curated** playlist as the subtrack list. Load a companion `.m3u` when one sits next to a local tune. **Local-only** (Modland/FTP does not ship these — dropped). Requires TODO_21. Independent of TODO_30.

## Context

`GmePlugin::open` (`src/player/plugins/GmePlugin.cpp`) reads the whole file into memory and hands it to `gme_open_data` — it never overlays any playlist, so even a local tune with a companion `.m3u` gets no length or curated names. After TODO_21, `startTrack` derives `m_duration` from `gme_info_t.length` (else `intro_length + loop_length`, else `0`). `gme_load_m3u`/`gme_load_m3u_data` populate exactly those `length`/`song` fields per playlist entry, and afterwards `gme_track_count` returns the **playlist** size (the curated track count, e.g. 14 curated of 17 raw) — so no change to the duration derivation or subtrack model is needed; the existing reads pick it all up.

Two real-world `.m3u` layouts exist and both must work:

1. **Combined** — a single `<stem>.m3u` next to the tune (`game.nsf` + `game.m3u`), one line per track. `gme_load_m3u(path)` parses it directly.
2. **Exploded** (common Zophar dumps) — no combined file, but several `.m3u` files in the tune's directory, each named by a track title (`01 BGM #01.m3u`) and holding **one** line that references the tune by filename + track index. These are single-entry playlists; loading one only exposes that one track, so they must be **gathered and concatenated** into a combined buffer and loaded via `gme_load_m3u_data`.

## The fix (implemented)

A private best-effort helper `GmePlugin::overlaySiblingM3u(tunePath)`, called from `open()` right after `m_emu.reset(raw)` and **before** `m_trackCount = gme_track_count(...)` so the curated count is picked up:

- **Combined first**: `<stem>.m3u`; if it is a regular file → `gme_load_m3u`; on success done, on error log and fall through.
- **Exploded fallback**: iterate `tunePath.parent_path()` (`directory_iterator` with the `error_code` overloads, never throwing out of the helper); for each regular `.m3u` file (case-insensitive) other than the combined one, read it and keep every line whose filename field (before the first `"::"`) case-insensitively equals `tunePath.filename()`. Sort the kept lines by their source `.m3u` filename (album order), concatenate newline-separated, and `gme_load_m3u_data`.
- Strictly best-effort throughout: any missing file, unreadable directory, or malformed/rejected playlist is `SDL_Log`ged at most and never fails the open. Uses the filesystem `error_code` overloads (the outer `open()` try/catch is not relied on for control flow).

## Files changed

1. **`src/player/plugins/GmePlugin.h`** — declare `overlaySiblingM3u`.
2. **`src/player/plugins/GmePlugin.cpp`** — the helper + its call in `open()`.

## Docs

- **`docs/audio.md`** — GmePlugin now overlays a companion `.m3u` (both layouts) at `open()`, supplying the curated subtrack list + the `length`/`song` fields TODO_21 reads.

## Coordination

- Requires **TODO_21** (real-length-only duration; the m3u supplies that length). Independent of **TODO_30** (SID lengths). No filesystem/FTP changes.

## Verification

- Desktop + Switch builds green.
- A local tune with a **combined** `<stem>.m3u`, and one with the **exploded** per-track layout (verified against a `CGB-AYPE-USA.gbs` GBS + 14 per-track `.m3u` files: libgme reports 14 curated subtracks named `BGM #01…`/`Jingle #01…` with lengths 2:29, 0:37, 0:33, 0:20, 0:32, 0:12, 0:04, 0:01, …), both show real per-subtune durations and curated titles.
- The same tune **without** any `.m3u` is unchanged (raw tracks, open-ended per TODO_21).
- Non-GME formats and other decoders are unaffected.
