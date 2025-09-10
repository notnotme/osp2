# OSP2

OSP2 is a chiptune player for desktop Linux and Nintendo Switch (homebrew), written in C++20 on top of SDL2, OpenGL, and Dear ImGui.

It plays tracker music through a plugin-based decoder architecture: each supported library is wrapped in a `PlayerPlugin`, and plugins are selected by file extension. Today the [libopenmpt](https://lib.openmpt.org/libopenmpt/) plugin covers the classic module formats (MOD, S3M, XM, IT, and everything else libopenmpt handles); libgme, libsidplayfp, and libsc68 plugins are planned.

## Features

- Module playback via libopenmpt — 48 kHz float32 stereo, pull-model SDL audio
- File browser with transport controls (play/pause, stop, previous/next) and end-of-track auto-advance
- Single codebase for desktop Linux and Nintendo Switch, with a fixed 1280×720 UI matching the Switch screen

See the roadmap in [docs/todos/TODO.md](docs/todos/TODO.md) for what's coming next (UI redesign with themes, threaded file browser, per-format metadata display, persisted settings and plugin configuration).

## Building

Clone with the Dear ImGui submodule:

```sh
git clone <repo-url>
cd osp2
git submodule update --init
```

### Desktop (Linux)

Requires SDL2, SDL2_image, OpenGL, glad, and libopenmpt installed system-wide.

```sh
cmake -B build
cmake --build build
```

Run from the repository root — asset paths are relative (`romfs/`):

```sh
./build/OSP2
```

### Nintendo Switch

Requires [devkitPro](https://devkitpro.org/) with the Switch toolchain and portlibs (SDL2, SDL2_image, glad, libopenmpt). With devkitPro installed at `/opt/devkitpro`:

```sh
mkdir -p cmake-build-switch && cd cmake-build-switch
source /opt/devkitpro/switchvars.sh
cmake -G"Unix Makefiles" -DCMAKE_C_FLAGS="$CFLAGS $CPPFLAGS" -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake ..
make -j$(nproc)
```

This produces `OSP2.nro` with `romfs/` embedded, ready to run through hbmenu or nxlink.

## Project layout

| Path | Contents |
|---|---|
| `src/main.cpp` | Platform lifecycle: SDL/OpenGL/ImGui init, event loop, render loop |
| `src/gui/` | Presentation-only `Gui` (Dear ImGui), sprite atlas rendering |
| `src/filesystem/` | Directory listings (`FileSystem`, `FileEntry`) |
| `src/player/` | Playback core: `PlayerController`, decoder plugins (`plugins/`) |
| `external/imgui/` | Dear ImGui, pristine git submodule |
| `romfs/` | Runtime assets: fonts, sprite atlas, test music |
| `docs/` | Per-domain architecture docs with class diagrams, plus the TODO backlog |

Architecture details — including the audio threading contract and how to add a decoder plugin — are documented per domain in [docs/](docs/) and in [CLAUDE.md](CLAUDE.md).

## Acknowledgements

- [SDL2](https://www.libsdl.org/) & SDL2_image
- [Dear ImGui](https://github.com/ocornut/imgui)
- [libopenmpt](https://lib.openmpt.org/libopenmpt/)
- [glad](https://github.com/Dav1dde/glad)
- [devkitPro](https://devkitpro.org/) (Switch homebrew toolchain)
- Roboto and Material Symbols fonts (Google)
