# TODO_26 — Remove the default bundled sound (`romfs/music/`)

> `romfs/music/test.s3m` is a leftover test asset, played by a hardcoded fallback when PLAY_PAUSE is pressed with nothing loaded. Delete the asset (and folder) and remove the fallback. Single chunk.

## Context

`Application::handleButtonClick` (`src/Application.cpp:33-62`) has, in the PLAY_PAUSE → STOPPED branch (`:44-45`):

```cpp
// TODO(temporary): hardcoded test track until FileSystem returns real directories.
m_player.play(assetPath("music/test.s3m"));
```

That "temporary" hack predates the real threaded FileSystem browser (TODO_4), which now provides real directories — so the fallback is obsolete. The asset is `romfs/music/test.s3m`; the Switch build packages the whole `romfs/` tree into the NRO's RomFS automatically, so removing the folder is enough (no CMake change). No other code references `music/` beyond this line (the `user/default_folder` setting is unrelated).

## The fix (single chunk)

- Delete `romfs/music/test.s3m` and the now-empty `romfs/music/` directory.
- Remove the hardcoded fallback in `Application.cpp:44-45`. With nothing loaded, PLAY_PAUSE while STOPPED becomes a no-op (there is no track to resume; `m_player.play()` with no path only resumes an existing track).

## Files to change

1. **`romfs/music/`** — delete the folder and `test.s3m`.
2. **`src/Application.cpp`** — remove the fallback `play(assetPath("music/test.s3m"))` in the PLAY_PAUSE/STOPPED branch (`:44-45`).

## Docs

- No domain diagram change. Mention in the commit body that the temporary test-track hack is removed.

## Verification

- Desktop + Switch builds green; the NRO packages without `romfs/music/` and there is no missing-asset error at runtime.
- PLAY_PAUSE with nothing loaded does nothing (no crash, no attempt to open a deleted file).
- Selecting and playing a real file from the browser still works normally.
