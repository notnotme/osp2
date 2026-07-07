# TODO_20 — Non-UTF-8 track metadata (charset transcoding + CJK font)

> Decoder metadata (title, author, copyright, comment) is captured in each format's **legacy charset** and handed to Dear ImGui as if it were UTF-8, so any non-ASCII text renders as `?`. Two chunks: transcode at the plugin boundary (20a), then bundle a CJK-capable font so East-Asian text actually has glyphs (20b). Independent of the other backlog items; requires TODO_9 (Gme/Sid/Sc68 plugins) and TODO_5 (typed metadata) to already exist.

## Context

Dear ImGui expects UTF-8. Its UTF-8 decoder maps every invalid byte to the fallback glyph (`?`), and maps valid codepoints with no glyph in the loaded font to the font's fallback. OSP2 hits **both** failure modes for non-Latin metadata.

**Encoding (20a).** Each decoder library returns strings in the charset native to its format, and the plugins store those raw bytes into `TrackMetadata` (`src/player/Metadata.h`) during `open()`:

- **GME / NSF, GBS, KSS, …** — the NSF header's name/artist/copyright fields are **Shift-JIS** (or ASCII). libgme returns the raw bytes. Confirmed example in cache: `Nintendo Sound Format/GPM/hirake ponkikki oppai ga ippai.nsf` — the title field (offset `0x0E`) is `82 a8 82 c1 82 cf 82 a2 …` = Shift-JIS おっぱいがいっぱい, shown in the UI as `????`.
- **SID (libsidplayfp / HVSC)** — the STIL/PSID text fields are **ISO-8859-1 (Latin-1)**; accented Latin (é, ü, ñ) currently corrupts.
- **SC68 (libsc68 / file68)** — Atari ST / Amiga tags are typically **Latin-1** or the **Atari ST** codepage.
- **OpenMPT** — libopenmpt already returns **UTF-8** (it transcodes internally), so module metadata is fine and must be left untouched.

The fix belongs at the **plugin boundary**, where the source charset is known: a shared `toUtf8(bytes, sourceCharset)` helper, called by each plugin's `open()` for the fields it captures. Latin-1→UTF-8 is a trivial per-byte expansion; Shift-JIS and Atari ST need lookup tables (or a conversion routine). **Open question — the conversion backend:** `libiconv` (present on desktop, uncertain as a devkitPro portlib for the Switch), a small header-only converter, or hand-rolled tables per charset. Resolve this first; it decides the shape of 20a and must keep the Switch build green (portlib or vendored, not desktop-only).

**Font (20b).** `Platform::loadFonts()` (`src/Platform.cpp:257`) loads **Roboto-Regular** (default Latin ranges) merged with **Material Symbols** (icons). Roboto has no CJK glyphs, so even after 20a transcodes Shift-JIS→UTF-8 correctly, Japanese renders as tofu boxes (□). Rendering CJK needs a CJK font merged in with the right glyph ranges (e.g. `ImFontAtlas::GetGlyphRangesJapanese`). The asset is large: **subset** it (kana + the common-kanji range ImGui already curates) to respect the Switch `romfs/` budget, and add it under `romfs/font/` like the existing fonts. After 20a, Latin metadata is already correct with Roboto; 20b is what unlocks Japanese/CJK.

## Task chunks (implement, verify, and commit one at a time)

- [x] **20a — Charset transcoding to UTF-8 at the plugin boundary**: add a shared `toUtf8` helper (new `src/player/Charset.{h,cpp}` or similar — remember the CMake `add_executable` entry for the new `.cpp`) taking bytes + a source-charset enum (`ShiftJis`, `Latin1`, `AtariSt`, …). Call it in `GmePlugin::startTrack`/`open` (Shift-JIS), `SidPlugin::open` (Latin-1), and `Sc68Plugin::open` (Latin-1 / Atari ST) for every captured string; leave `OpenMptPlugin` (already UTF-8) untouched. Decide the conversion backend (see Context) and keep the **Switch build green**. Verify: the NSF above shows `????` replaced by correct text once a CJK font exists (until then, valid-but-glyphless — no crash, no `?`); a Latin-1 SID with accents renders é/ü correctly with the current font; ASCII and OpenMPT metadata are unchanged.
- [ ] **20b — CJK-capable font**: add a subset CJK font under `romfs/font/`, merge it in `Platform::loadFonts()` with the appropriate glyph ranges (Japanese at minimum; consider full CJK if size allows), and verify the atlas builds on both platforms. Mind the `romfs/` size on Switch — subset aggressively. Verify on Switch hardware (ask before deploying): the Shift-JIS NSF title now renders as Japanese glyphs, not tofu; Latin and icon glyphs still render; startup/atlas build time and memory stay acceptable.

Each chunk ends with green desktop + Switch builds, docs updated, user verification (20b needs a Switch hardware test), then a commit. Run cpp-reviewer on the diff before committing.

## Files to change

1. **`src/player/Charset.{h,cpp}`** (new) — `toUtf8(std::string_view, Charset)`; add the `.cpp` to `add_executable` in CMakeLists.txt.
2. **`src/player/plugins/GmePlugin.cpp`**, **`SidPlugin.cpp`**, **`Sc68Plugin.cpp`** — transcode captured metadata strings; `OpenMptPlugin` unchanged.
3. **`src/Platform.cpp`** — merge the CJK font + glyph ranges in `loadFonts()` (20b).
4. **`romfs/font/`** — the subset CJK font asset (20b); note it in `THIRD_PARTY_NOTICES.md` with its license (e.g. Noto SIL OFL).

## Docs

- **`docs/audio.md`** — the plugin-boundary transcoding step (which plugin uses which source charset; OpenMPT already UTF-8) and the shared `toUtf8` helper.
- **`docs/platform.md`** / **`docs/ui.md`** — the CJK font merge + glyph ranges and the Switch romfs-size note.

## Coordination

- **Requires TODO_9** (Gme/Sid/Sc68 plugins) and **TODO_5** (typed metadata). Independent of TODO_19. The conversion-backend decision (iconv vs vendored tables) is the gating unknown for the Switch — settle it before starting 20a.

## Verification

- Desktop + Switch builds green (per CLAUDE.md).
- **20a**: Latin-1 accents render correctly with the current font; the Shift-JIS NSF no longer shows `?` (valid codepoints, glyphless until 20b); OpenMPT/ASCII metadata unchanged; no crash on malformed byte sequences.
- **20b** (Switch hardware, ask before deploying): the Shift-JIS NSF title renders as Japanese; Latin + icon glyphs still render; atlas build and romfs size stay within budget.
