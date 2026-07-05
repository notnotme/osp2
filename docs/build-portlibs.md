# Building decoder libraries

Some decoder libraries OSP2 links are not packaged for one or both targets and are built from
source by the maintainer. This file records exactly how, so the builds are reproducible. These are
**environment prerequisites**, not part of any code chunk — the CMake stanzas assume `pkg-config`
resolves each library in the target environment.

## libsidplayfp (+ libresidfp) — SID playback

`SidPlugin` uses the `sidplayfp` engine with the **ReSIDfp** emulation (accurate, the gold standard).
In libsidplayfp **v3** the ReSIDfp emulation was split into a separate **`libresidfp`** library that
libsidplayfp detects via `pkg-config` at configure time and exposes as `ReSIDfpBuilder`
(`<sidplayfp/builders/residfp.h>`). If `libresidfp` is absent when libsidplayfp is configured, only
the lightweight built-in `SIDLiteBuilder` is compiled in and `residfp.h` is **not** installed.

**Build order is therefore fixed: `libresidfp` first (installed), then `libsidplayfp`.** Both targets
must have `libresidfp` so `ReSIDfpBuilder` exists on both. Versions used: libresidfp 1.1.1,
libsidplayfp 3.0.3a.

Sources (upstream, cloned to a scratch dir; not osp2 submodules):
- `https://github.com/libsidplayfp/libresidfp`
- `https://github.com/libsidplayfp/libsidplayfp` (run `git submodule update --init` — the exsid/usbsid
  driver submodules provide an m4 macro dir referenced by `configure.ac`)

### Desktop (native, installs into `/usr/local`)

```sh
# In each source tree, libresidfp FIRST:
autoreconf -fi
./configure --disable-shared --enable-static
make -j16
sudo make install          # libresidfp.pc lands in /usr/local/lib/pkgconfig
# Then libsidplayfp (its configure now finds libresidfp via pkg-config):
git submodule update --init
autoreconf -fi
./configure --disable-shared --enable-static --without-exsid --without-usbsid
make -j16
sudo make install
```

Verify: `pkg-config --exists libresidfp libsidplayfp` and
`ls /usr/local/include/sidplayfp/builders/residfp.h`.

> Note: a system (apt) `libsidplayfp-dev` in `/usr/include` will collide with the `/usr/local` v3
> headers (mismatched `sidbuilder.h` vs `residfp.h`), causing "abstract class" compile errors.
> Remove the apt package so only the `/usr/local` v3 install remains.

### Switch (devkitA64 portlib, installs into `/opt/devkitpro/portlibs/switch`)

Same order (libresidfp first), cross-compiled. See the full recipe in the memory note
`switch-portlib-build-recipe`; in short, per source tree:

```sh
source /opt/devkitpro/switchvars.sh
export PKG_CONFIG=aarch64-none-elf-pkg-config
autoreconf -fi   # for libsidplayfp: git submodule update --init first
./configure --host=aarch64-none-elf --prefix="$PORTLIBS_PREFIX" \
            --disable-shared --enable-static --disable-tests \
            --without-exsid --without-usbsid       # exsid/usbsid: libsidplayfp only
make -j16
sudo env PATH="$PATH" PORTLIBS_PREFIX="$PORTLIBS_PREFIX" make install
```

Verify: `aarch64-none-elf-pkg-config --exists libresidfp libsidplayfp`.

These builds are direct-from-source (not `dkp-makepkg` PKGBUILDs), so `dkp-pacman` does not track
them — a toolchain reinstall requires rebuilding from this recipe.

## libsc68 (+ file68 + unice68) — Atari ST / Amiga / SNDH playback

`Sc68Plugin` links **libsc68**, which is a meta-package of three libraries built together in
dependency order: **unice68** (ICE depacker) → **file68** (loader; optional zlib for gzipped files,
optional curl) → **libsc68** (the 68k emulator + player). Each ships its own `pkg-config` file
(`unice68.pc`, `file68.pc`, `sc68.pc`); `pkg-config sc68` pulls the whole chain
(`-lsc68 -lfile68 -lunice68 -lz`). Version used: 3.0.0.42 (SVN r713 tarball, unpacked to a scratch
dir; **not** an osp2 submodule).

The tree is autotools-from-SVN, so it must be **bootstrapped first** (generates the `configure`
scripts): from the top of the source tree run `sh tools/svn-bootstrap.sh` (needs autoconf ≥2.63,
automake ≥1.11, libtool ≥2.2.6, pkg-config). This also symlinks the shared m4 macros across the
sub-packages.

We build **libraries only** — no CLI player (`sc68`/`info68`), no dev tools, no docs, no curl:
`--disable-sc68-bins --disable-sc68-tools --disable-sc68-doc --without-curl`. The `unice68` CLI must
also be turned off with **`--disable-unice68-cli`** (see the basename note below). `as68` — a 68k
assembler that generates `libsc68/sc68/trap68.h` (the TOS trap emulator) — is built for the **host**;
`--enable-as68` forces it. Because it is a host tool, **the native (desktop) build must run first**:
it generates `libsc68/sc68/trap68.h` into the source tree, which the cross build then reuses (a cross
build cannot run `as68`). `hexdump` must be on PATH (as68 pipes through it).

### The newlib `basename` gap (Switch only)

devkitA64's newlib **declares** `basename` (in `<string.h>`/`<libgen.h>`) but ships **no libc symbol**
for it, so autoconf's link-test leaves `HAVE_BASENAME` undefined. That breaks the cross build two ways:
`unice68.c` compiles a conflicting `static basename`, and `libsc68/src/api68.c` does
`#ifndef HAVE_BASENAME → #include "libc68.h"` (a header from the unbuilt `sc68-libc` sub-package).
Fix **without patching sc68 sources**: force `ac_cv_func_basename=yes` on the cross configure so both
use the system declaration. `unice68`'s only `basename` caller is its CLI `main` → disabled via
`--disable-unice68-cli`. That leaves exactly one unresolved `U basename` in `libsc68.a` (from
`api68.c`), which OSP2 satisfies at final link with a tiny Switch-only stub (see `SwitchCompat.c` /
the `Sc68Plugin` CMake stanza).

### Desktop (native, installs into `/usr/local`) — run first, generates trap68.h

```sh
sh tools/svn-bootstrap.sh                 # once, from the source tree top
mkdir -p _build/desktop && cd _build/desktop
../../configure --disable-sc68-bins --disable-sc68-tools --disable-sc68-doc \
                --enable-as68 --without-curl --prefix=/usr/local
make -j16                                 # builds as68 + trap68.h + the 3 libs
sudo make install
```

Verify: `pkg-config --modversion sc68` → `3.0.0.42`.

### Switch (devkitA64 portlib, installs into `/opt/devkitpro/portlibs/switch`)

Cross build reuses the `trap68.h` the desktop build generated (no `as68`, `cross_compiling=yes`).

```sh
source /opt/devkitpro/switchvars.sh
export PKG_CONFIG=aarch64-none-elf-pkg-config
export ac_cv_func_basename=yes            # newlib basename gap (see above)
mkdir -p _build/switch && cd _build/switch
../../configure --host=aarch64-none-elf --prefix="$PORTLIBS_PREFIX" \
                --disable-shared --enable-static \
                --disable-sc68-bins --disable-sc68-tools --disable-sc68-doc \
                --without-curl --disable-unice68-cli
make -j16
# install re-runs aarch64-none-elf-gcc-ranlib on the .a, so PATH (from switchvars)
# must survive sudo:
source /opt/devkitpro/switchvars.sh && sudo env PATH="$PATH" make install
```

Verify: `aarch64-none-elf-pkg-config --modversion sc68` → `3.0.0.42`; the static link line is
`-lsc68 -lfile68 -lunice68 -lz`.

These builds are direct-from-source (not `dkp-makepkg` PKGBUILDs), so `dkp-pacman` does not track them.
