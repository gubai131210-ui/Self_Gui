#!/usr/bin/env python3
"""Generate Windows multi-size ICO from the Selt app icon design (no SVG renderer required)."""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "resources" / "icons" / "app_icon.ico"
SIZES = (16, 24, 32, 48, 64, 128, 256)


def draw_icon(size: int) -> Image.Image:
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    s = size / 256.0

    def xy(*vals: float):
        return tuple(int(round(v * s)) for v in vals)

    # Background
    d.rounded_rectangle(xy(16, 16, 240, 240), radius=max(2, int(44 * s)), fill=(40, 43, 48, 255), outline=(62, 66, 74, 255), width=max(1, int(4 * s)))
    # Image frame
    d.rounded_rectangle(xy(52, 58, 204, 170), radius=max(1, int(14 * s)), fill=(32, 34, 38, 255), outline=(110, 115, 125, 255), width=max(1, int(6 * s)))
    # ROI
    d.rounded_rectangle(xy(78, 78, 156, 136), radius=max(1, int(6 * s)), outline=(255, 140, 0, 255), width=max(1, int(8 * s)))
    # Measure line
    d.line(xy(92, 150, 178, 98), fill=(64, 150, 230, 255), width=max(1, int(6 * s)))
    r = max(1, int(7 * s))
    d.ellipse([xy(92, 150)[0] - r, xy(92, 150)[1] - r, xy(92, 150)[0] + r, xy(92, 150)[1] + r], fill=(56, 180, 90, 255))
    d.ellipse([xy(178, 98)[0] - r, xy(178, 98)[1] - r, xy(178, 98)[0] + r, xy(178, 98)[1] + r], fill=(56, 180, 90, 255))
    # Flow nodes
    nr = max(1, int(12 * s))
    for cx in (68, 128, 188):
        c = xy(cx, 198)
        d.ellipse([c[0] - nr, c[1] - nr, c[0] + nr, c[1] + nr], fill=(255, 140, 0, 255))
    d.line(xy(80, 198, 116, 198), fill=(230, 232, 235, 255), width=max(1, int(6 * s)))
    d.line(xy(140, 198, 176, 198), fill=(230, 232, 235, 255), width=max(1, int(6 * s)))
    return img


def main() -> None:
    images = [draw_icon(sz) for sz in SIZES]
    OUT.parent.mkdir(parents=True, exist_ok=True)
    images[0].save(OUT, format="ICO", sizes=[(im.width, im.height) for im in images], append_images=images[1:])
    # Pillow ICO save is more reliable when saving from the largest with sizes=
    images[-1].save(OUT, format="ICO", sizes=[(sz, sz) for sz in SIZES])
    print(f"Wrote {OUT}")


if __name__ == "__main__":
    main()
