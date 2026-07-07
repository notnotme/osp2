# TODO_29 — NSF/GME durations from a sibling `.m3u`

> TODO_21 made bare NSF tracks show open-ended time because libgme reports no real length. Many NSF (and other GME formats) ship with a companion **`.m3u` playlist** that carries per-subtune titles *and* lengths in the "NSF M3U" convention (`tune.nsf::NSF,1,Title,2:30`). libgme can overlay that data via `gme_load_m3u`. Load a sibling `.m3u` when one sits next to the tune (local libraries **and** Modland downloads), so those tracks get accurate per-subtune durations again. Single feature; requires TODO_21. Independent of TODO_30.

## Context

`GmePlugin::open` (`src/player/plugins/GmePlugin.cpp:67`) reads the whole file into memory and hands it to `gme_open_data` — it **never** passes a path to libgme, so `gme_load_m3u` is not called today. As a result, even a **local** NSF with a sibling `.m3u` gets no length or per-track names. After TODO_21, `startTrack` (`GmePlugin.cpp:148`) derives `m_duration` from `gme_info_t.length` (else `intro_length + loop_length`, else `0`). `gme_load_m3u` populates exactly those `length`/`song` fields per track, so **no change to the duration derivation is needed** — the existing read picks the m3u values up automatically once the playlist is overlaid.

For remote sources, `FtpDataSource::fetchFile` (`src/filesystem/FtpDataSource.cpp:534`) downloads only the single requested file to the cache mirror (`.part` → rename on success). A sibling `.m3u` is never fetched, so a cached Modland NSF can't be enriched even if the archive hosts one.

## The fix

Two small, independent pieces:

1. **`GmePlugin::open`** — after a successful `gme_open_data`, look for a sibling playlist next to `path` (`path` with extension replaced by `.m3u`). If it exists, call `gme_load_m3u(m_emu.get(), m3uPath.string().c_str())`; on error just `SDL_Log` and continue (the m3u is optional enrichment — a malformed/absent playlist must never fail the open). This alone fixes local libraries. Overlay happens **before** `startTrack(0)` so the first subtrack already reflects the m3u.
2. **`FtpDataSource::fetchFile`** — after the primary file commits successfully, if the requested file's extension is one GME reads m3u for (at least `nsf`, `nsfe`, `kss`, `gbs`, `hes`, `sap`, `ay` — keep a small set), best-effort fetch the sibling `<stem>.m3u` into the same cache dir (reuse the download/commit path; a 404 or any error is ignored and must not fail or slow the primary result). The GmePlugin sibling-load in (1) then finds it on the cached copy.

## Files to change

1. **`src/player/plugins/GmePlugin.cpp`** — sibling `.m3u` detection + `gme_load_m3u` in `open()` (`:67`).
2. **`src/filesystem/FtpDataSource.cpp`** — opportunistic sibling `.m3u` fetch in `fetchFile()` (`:534`), scoped to GME-m3u extensions, best-effort.

## Docs

- **`docs/audio.md`** — note that GME now overlays a sibling `.m3u` (`gme_load_m3u`) when present, which supplies the `length`/`song` fields TODO_21 reads; absent/malformed playlists are ignored.
- **`docs/filesystem.md`** — note the best-effort sibling-`.m3u` fetch alongside GME tunes and that it never affects the primary download's success.

## Coordination

- Requires **TODO_21** (real-length-only duration; the m3u simply supplies that length). Touches `FtpDataSource.cpp` — serialise with any other FTP work. Independent of **TODO_30** (SID lengths).

## Caveats

- Payoff for the remote path depends on Modland actually hosting `.m3u` siblings in its GME trees — not guaranteed archive-wide. The mechanism is cheap and degrades to today's behaviour when no playlist is present, and it benefits any local library that has them regardless.

## Verification

- Desktop + Switch builds green.
- A local NSF **with** a sibling `.m3u` shows real per-subtune durations and titles; the same NSF **without** the m3u is unchanged (open-ended, per TODO_21).
- Downloading that NSF from an FTP source that hosts the `.m3u` produces the same enriched durations; a source without it still plays (open-ended), with no error and no perceptible extra delay.
- Non-GME formats and GME formats with no m3u are unaffected.
