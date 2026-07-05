# UI domain

Presentation layer in `src/gui/`. `Gui` is stateless apart from the sprite atlas texture: each frame it receives a `UiState` view model (all data to render) and a `UiActions` bundle (callbacks to report intent). It never touches the player or filesystem directly — `Application` builds `UiState`/`UiActions`, main.cpp just forwards them (see [application.md](application.md)).

```mermaid
classDiagram
    class Gui {
        -unordered_map~string, Sprite~ m_sprites
        -GLuint m_texture
        +initialize(basePath)
        +finalize()
        +drawUserInterface(state, actions)
        -drawMainMenuBar(file)
        -drawCurrentPath(path)
        -drawFileBrowser(files, onFileClick, isWorking)
        -drawPlayerControls(onButtonClick)
        -drawTrackInformation()
        -drawTabsSection()
        -drawFileMetadata()
        -drawTabPlaylist()
        -drawTabSettings()
        -drawTabAbout()
    }

    class UiState {
        <<value object>>
        +string currentFile
        +string path
        +const vector~FileEntry~& files
        +bool isWorking
    }

    class UiActions {
        <<value object>>
        +function~void(ButtonId)~ onButtonClick
        +function~void(const FileEntry&)~ onFileClick
    }

    class Sprite {
        <<value object>>
        +float s
        +float t
        +float p
        +float q
        +int16 w
        +int16 h
    }

    class ButtonId {
        <<enumeration>>
        PLAY_PAUSE
        STOP
        NEXT
        PREVIOUS
    }

    Gui *-- Sprite : atlas entries from sprites.bin
    Gui ..> UiState : renders per frame
    Gui ..> UiActions : reports intent through
    UiState ..> FileEntry : non-owning view
    UiActions ..> ButtonId : onButtonClick
    UiActions ..> FileEntry : onFileClick
```

## Notes

- `UiState` (`src/gui/UiState.h`) is a per-frame value object, rebuilt each frame and never stored; its `files` member is a non-owning reference valid only for that frame. `UiActions` (`src/gui/UiActions.h`) is the callback bundle, wired once at startup. Both are produced by `Application`.
- `drawUserInterface(state, actions)` fans the view model out to the private draw helpers, which keep their focused per-widget parameters (`drawFileBrowser(state.files, actions.onFileClick, state.isWorking)`, etc.).

- Sprites are loaded in `initialize()` from `romfs/sprites/sprites.bin` (custom `SPSH` format) + `sprites.png` into one GL texture; `Sprite` holds the UV rect (s/t/p/q) and pixel size.
- Icon glyphs in labels (e.g. folder/file icons) are Material Symbols codepoints merged into the default font in main.cpp.
- Dear ImGui is a pristine git submodule at `external/imgui/` (pinned to v1.92.8). The Switch glad integration lives in `src/gui/imgui_impl_opengl3_glad.cpp` — a wrapper that includes `<glad/glad.h>` before the upstream OpenGL3 backend (`IMGUI_IMPL_OPENGL_LOADER_CUSTOM` skips the embedded loader on Switch).
