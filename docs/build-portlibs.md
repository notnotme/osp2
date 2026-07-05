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

## libsc68 — Atari ST / SNDH playback (planned)

Not yet built (prerequisite **P2** for `Sc68Plugin`). Absent on desktop and Switch. To be documented
here once built.
