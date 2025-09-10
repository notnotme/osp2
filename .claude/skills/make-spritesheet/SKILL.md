---
name: make-spritesheet
description: Build the sprite atlas (sprites.png + sprites.bin) from source PNGs and sprites.json coordinates. Use when adding/changing UI sprites, editing romfs/raw images, or regenerating romfs/sprites.
---

# Make spritesheet

Regenerates the runtime sprite atlas consumed by `Gui::initialize()` from the source assets in `romfs/raw/`.

## Usage

From the repository root:

```sh
python3 .claude/skills/make-spritesheet/scripts/make_spritesheet.py \
    romfs/raw/sprites.json romfs/raw romfs/sprites
```

Arguments: `<sprites.json> <source-image-dir> <output-dir>`. Add `--bin-only` to regenerate only `sprites.bin` (coordinates changed, images untouched). Requires Pillow (installed system-wide).

## Adding or changing a sprite

1. Put the source image in `romfs/raw/<name>.png` (RGBA).
2. Add/edit its frame in `romfs/raw/sprites.json`: `"<name>": {"x", "y", "w", "h"}` — pixel rect inside the atlas; grow `"size"` if it doesn't fit. Leave a 1px gap between frames (existing sheet does; GL_NEAREST sampling, so no wider gutter needed).
3. Run the script (above). It validates: name < 32 bytes, frame fits the atlas, source image size matches `w`/`h`.
4. Use it in code: `m_sprites.at("<name>")` in Gui (S/T/P/Q are the UV corners for `ImGui::Image*`, `w`/`h` the pixel size).
5. Verify by running the app; commit `romfs/raw/*` and `romfs/sprites/*` together.

## sprites.bin format (what the script writes, what Gui parses)

Little-endian throughout:

| Field | Type | Meaning |
|---|---|---|
| signature | `char[4]` | `"SPSH"` |
| count | `int32` | number of sprites |
| per sprite: name | `char[32]` | NUL-padded sprite name |
| s, t | `float32 ×2` | top-left UV: `x/W`, `y/H` (W×H = atlas size) |
| p, q | `float32 ×2` | bottom-right UV: `(x+w)/W`, `(y+h)/H` |
| w, h | `int16 ×2` | sprite size in pixels |

No pixel insets/half-texel offsets; UVs are exact edges. Entry order in the file is irrelevant (loaded into an `unordered_map`).

## Notes

- The script's output was verified to reproduce the shipped `romfs/sprites/` exactly (identical sprite records and pixel-identical PNG); only entry order in the bin differs from the original tool, which is harmless.
- If the parser in `Gui::initialize()` or the `Sprite` struct changes, update the format table here and the script together.
