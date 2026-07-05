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
        +create()
        +destroy()
        +play(path) bool
        +play()
        +pause()
        +stop()
        +getState() PlayerState
        +getCurrentFileName() string
        +getCurrentPath() path
        +getCurrentTitle() string
        +getStatus() PlaybackStatus
        +isSupported(path) bool
        +consumeTrackEnded() bool
        -audioCallback(userdata, stream, len)$
        -decode(stream, len)
        -findPluginFor(path) PlayerPlugin*
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
    }

    class OpenMptPlugin {
        -int m_sampleRate
        -vector~string~ m_extensions
        -unique_ptr~openmpt_module~ m_module
    }

    class PlaybackStatus {
        <<value object>>
        +PlayerState state
        +string title
        +string fileName
        +double positionSeconds
        +double durationSeconds
    }

    PlayerController --> PlayerState : state machine
    PlayerController ..> PlaybackStatus : getStatus() snapshot
    PlayerController "1" o-- "*" PlayerPlugin : owns, dispatches by extension
    PlayerPlugin <|-- OpenMptPlugin : libopenmpt (mod, xm, s3m, it, ...)
```

## Threading

- The SDL audio thread pulls samples via `audioCallback` → `decode()` (48 kHz, float32 interleaved stereo, `AUDIO_F32SYS`).
- `m_mutex` guards `m_state` and `m_activePlugin` (including the decoder inside it); locked by the callback and by play/pause/stop/getters.
- `getStatus()` returns a `PlaybackStatus` (`src/player/PlaybackStatus.h`) snapshot — state, title, filename, position, duration — built under a **single** `m_mutex` lock (it inlines the reads rather than calling the re-locking getters, since `m_mutex` is non-recursive). The plugin virtuals `getPosition()`/`getDuration()` (seconds) read the shared decoder, so they are contractually **only** called under `m_mutex`. Position/duration are `0` when stopped or unknown.
- Pause is controller state only — the device runs for the whole app lifetime and the callback emits silence when not PLAYING.
- End of track: the audio thread flips state to STOPPED and sets `m_trackEnded` (atomic); the main loop consumes it once per frame (`consumeTrackEnded()`) to auto-advance. Track teardown (`close()`) never happens on the audio thread.
- `destroy()` closes the audio device before destroying plugins.

## Adding a decoder plugin

One class under `src/player/plugins/` implementing `PlayerPlugin`, one `emplace_back` in `PlayerController::create()` (registration order = dispatch priority on extension overlap), one CMake stanza. Planned: libgme, libsidplayfp, libsc68.
