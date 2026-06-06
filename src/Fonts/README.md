# Fonts

Glyph tables compiled into the CJK firmware variants. Not used by the stock
build.

## Currently shipped

| File | Source | Format | Size | Used by |
| --- | --- | --- | --- | --- |
| `ASC12.h` | Fusion Pixel Font 12px monospaced (Latin) | 6×12 bitmap, 12 B/glyph, 128 glyphs | 1.5 KB | CJK build (ASCII slots) |
| `GB2312_L1.h` | Fusion Pixel Font 12px monospaced (zh\_hans) | 12×12 bitmap, 24 B/glyph, 5170 glyphs (GB2312 zones 1-55) | 121 KB | CJK build (CJK slots) |

The name `GB2312_L1` is kept for back-compat with the original 16×16 cut;
the actual content covers symbols/punctuation (zone 1), full-width ASCII
(zone 3), kana (4-5), Greek/Cyrillic (6-7), pinyin (8), drawing chars (9),
and all of Level-1 hanzi (16-55). Level 2 (zones 56-87) is omitted to save
flash.

## License

Both tables are derived from [Fusion Pixel Font](https://github.com/TakWolf/fusion-pixel-font)
by TakWolf, released under the **SIL Open Font License 1.1**. The license
text lives at `OFL.txt`; per-source attributions for the upstream fonts that
Fusion Pixel merges (Ark Pixel, Cubic 11, Galmuri) are in `licenses/`.

OFL permits embedding the font into any project — including commercial and
closed-source — provided the license file is shipped alongside the font
and the font itself isn't sold standalone. Both conditions are satisfied
here.

## Regenerating

```bash
# 1. Download Fusion Pixel Font 12px monospaced BDF release archive
curl -sLo /tmp/fp12.zip "https://github.com/TakWolf/fusion-pixel-font/releases/download/2026.05.07/fusion-pixel-font-12px-monospaced-bdf-v2026.05.07.zip"
mkdir -p /tmp/fusion-pixel/extracted && unzip -q /tmp/fp12.zip -d /tmp/fusion-pixel/extracted

# 2. Run the converter
python3 scripts/convert_fusion_pixel_12.py
```

If you bump the font version, also update `OFL.txt` and the files in
`licenses/` from the new release's `LICENSE/` directory.

## Legacy / alternate path

`scripts/extract_gb2312_l1.py` is a leftover from the first cut that pulled
glyphs from `M5StickCPlus`'s bundled HZK16 (16×16). Switched away because
12×12 is significantly denser on the 135 px wide screen. Kept in-tree as
documentation; not currently invoked.
