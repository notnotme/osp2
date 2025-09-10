#!/usr/bin/env python3
"""Build the OSP2 sprite atlas (sprites.png + sprites.bin) from source PNGs and a sprites.json.

sprites.json format (see romfs/raw/sprites.json):
    {
      "frames": { "<name>": {"x": int, "y": int, "w": int, "h": int}, ... },
      "size":   { "w": int, "h": int }
    }

sprites.bin format (parsed by Gui::initialize()):
    "SPSH"                      4-byte signature
    int32 LE                    sprite count
    per sprite:
        char[32]                name, NUL-padded
        float32 LE x4           s = x/W, t = y/H, p = (x+w)/W, q = (y+h)/H  (W,H = atlas size)
        int16 LE x2             w, h in pixels
"""

import argparse
import json
import struct
import sys
from pathlib import Path

from PIL import Image

MAX_NAME = 32


def load_manifest(json_path: Path):
    manifest = json.loads(json_path.read_text())
    frames = manifest["frames"]
    atlas_w, atlas_h = manifest["size"]["w"], manifest["size"]["h"]
    for name, f in frames.items():
        if len(name.encode()) >= MAX_NAME:
            sys.exit(f"error: sprite name '{name}' exceeds {MAX_NAME - 1} chars")
        if f["x"] + f["w"] > atlas_w or f["y"] + f["h"] > atlas_h:
            sys.exit(f"error: sprite '{name}' does not fit in the {atlas_w}x{atlas_h} atlas")
    return frames, atlas_w, atlas_h


def write_bin(frames, atlas_w, atlas_h, out_path: Path):
    with out_path.open("wb") as out:
        out.write(b"SPSH")
        out.write(struct.pack("<i", len(frames)))
        for name, f in frames.items():
            x, y, w, h = f["x"], f["y"], f["w"], f["h"]
            out.write(name.encode().ljust(MAX_NAME, b"\0"))
            out.write(struct.pack(
                "<ffffhh",
                x / atlas_w, y / atlas_h,
                (x + w) / atlas_w, (y + h) / atlas_h,
                w, h,
            ))


def write_png(frames, atlas_w, atlas_h, src_dir: Path, out_path: Path):
    atlas = Image.new("RGBA", (atlas_w, atlas_h), (0, 0, 0, 0))
    for name, f in frames.items():
        src_path = src_dir / f"{name}.png"
        if not src_path.exists():
            sys.exit(f"error: missing source image {src_path}")
        image = Image.open(src_path).convert("RGBA")
        if image.size != (f["w"], f["h"]):
            sys.exit(f"error: {src_path} is {image.size[0]}x{image.size[1]}, "
                     f"but sprites.json declares {f['w']}x{f['h']}")
        atlas.paste(image, (f["x"], f["y"]))
    atlas.save(out_path)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("json_path", type=Path, help="sprites.json manifest")
    parser.add_argument("src_dir", type=Path, help="directory with the per-sprite source PNGs (<name>.png)")
    parser.add_argument("out_dir", type=Path, help="output directory for sprites.png + sprites.bin")
    parser.add_argument("--bin-only", action="store_true", help="only regenerate sprites.bin")
    args = parser.parse_args()

    frames, atlas_w, atlas_h = load_manifest(args.json_path)
    args.out_dir.mkdir(parents=True, exist_ok=True)

    write_bin(frames, atlas_w, atlas_h, args.out_dir / "sprites.bin")
    if not args.bin_only:
        write_png(frames, atlas_w, atlas_h, args.src_dir, args.out_dir / "sprites.png")

    what = "sprites.bin" if args.bin_only else "sprites.png + sprites.bin"
    print(f"wrote {what} ({len(frames)} sprites, atlas {atlas_w}x{atlas_h}) to {args.out_dir}")


if __name__ == "__main__":
    main()
