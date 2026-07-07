#!/usr/bin/env python3
# Produce romfs/font/NotoSansJP-Subset.ttf: Noto Sans JP subset to exactly the glyph
# set Dear ImGui's ImFontAtlas::GetGlyphRangesJapanese() requests — Latin + punctuation
# + kana + half/fullwidth forms + the ~2999 common kanji ImGui curates. Keeping the file
# to that set respects the Switch romfs budget; the runtime merges it with the same
# ranges in Platform::loadFonts().
#
# Source: "Noto Sans JP" variable TrueType (glyf outlines) from Google Fonts
# (https://fonts.google.com/noto/specimen/Noto+Sans+JP). glyf is chosen deliberately:
# stb_truetype 1.26 (bundled by ImGui) rasterizes glyf and CFF but NOT the CFF2 outlines
# of the system-packaged Noto Sans CJK VF, so that copy is unusable here.
#
# Requires fonttools + brotli. Regenerate (pass the downloaded VF; not committed):
#   python3 scripts/gen_cjk_subset.py path/to/NotoSansJP-VariableFont_wght.ttf
import re
import sys
from pathlib import Path

from fontTools import subset
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont

ROOT = Path(__file__).resolve().parent.parent
IMGUI_DRAW = ROOT / "external/imgui/imgui_draw.cpp"
DEFAULT_SRC = ROOT / "scratch-font/NotoSansJP-VariableFont_wght.ttf"
DEST = ROOT / "romfs/font/NotoSansJP-Subset.ttf"


def imgui_japanese_codepoints() -> set[int]:
    """Mirror GetGlyphRangesJapanese(): base ranges + accumulated kanji from 0x4E00."""
    text = IMGUI_DRAW.read_text()
    # Grab the first accumulative_offsets_from_0x4E00 array body (the Japanese one).
    body = re.search(
        r"accumulative_offsets_from_0x4E00\[\]\s*=\s*\{(.*?)\}", text, re.S
    ).group(1)
    offsets = [int(n) for n in re.findall(r"-?\d+", body)]

    cps: set[int] = set()
    base_ranges = [
        (0x0020, 0x00FF),
        (0x2000, 0x206F),
        (0x3000, 0x30FF),
        (0x31F0, 0x31FF),
        (0xFF00, 0xFFEF),
        (0xFFFD, 0xFFFD),
    ]
    for lo, hi in base_ranges:
        cps.update(range(lo, hi + 1))

    cp = 0x4E00
    for off in offsets:
        cp += off
        cps.add(cp)
    return cps


def main() -> None:
    src = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_SRC
    cps = imgui_japanese_codepoints()
    print(f"target codepoints: {len(cps)}")

    font = TTFont(src)
    # Pin the weight axis to Regular (the VF defaults to Thin) and drop gvar/HVAR overhead.
    if "fvar" in font:
        instantiateVariableFont(font, {"wght": 400}, inplace=True)

    subsetter = subset.Subsetter(
        options=subset.Options(
            layout_features="*",
            name_IDs="*",
            recalc_bounds=True,
            drop_tables=["DSIG"],
        )
    )
    subsetter.populate(unicodes=sorted(cps))
    subsetter.subset(font)

    DEST.parent.mkdir(parents=True, exist_ok=True)
    font.save(DEST)
    kb = DEST.stat().st_size / 1024
    print(f"wrote {DEST} ({kb:.0f} KiB)")


if __name__ == "__main__":
    main()
