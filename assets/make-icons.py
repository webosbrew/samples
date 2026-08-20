#!/usr/bin/env python3
"""Generate one launcher icon per sample.

All eight samples used to share a single icon, which makes them impossible to tell apart
on the home screen. Each now gets its own colour from the Material 500 palette, plus a
short label, grouped so that a family is recognisable at a glance:

    blues   starfish-media-pipeline
    greens  NDL
    orange  LGNC

Run after changing the sample list; the PNGs are committed so the build needs no Python.
"""
import os
from PIL import Image, ImageDraw, ImageFont

FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
SIZE = 80
SS = 8  # supersample factor, for clean edges on the rounded corners and triangle

# name -> (Material 500 colour, label)
ICONS = {
    "smp-acb-webos2":      ("#3F51B5", "w2"),   # indigo
    "smp-acb-webos3":      ("#2196F3", "w3"),   # blue
    "smp-acb-webos4":      ("#03A9F4", "w4"),   # light blue
    "smp-webos5":          ("#00BCD4", "w5+"),  # cyan
    "ndl-esplayer":        ("#4CAF50", "ESP"),  # green
    "ndl-directmedia-v1":  ("#009688", "v1"),   # teal
    "ndl-directmedia-v2":  ("#8BC34A", "v2"),   # light green
    "lgnc":                ("#FF9800", "LGNC"), # orange
}


def make(path, colour, label):
    n = SIZE * SS
    img = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([0, 0, n - 1, n - 1], radius=int(n * 0.22), fill=colour)

    # play triangle, sitting in the upper two thirds
    cx, cy = n * 0.5, n * 0.40
    h = n * 0.34
    w = h * 0.87
    d.polygon([(cx - w / 2, cy - h / 2), (cx - w / 2, cy + h / 2), (cx + w / 2, cy)],
              fill=(255, 255, 255, 255))

    # label underneath, scaled down as it gets longer so it always fits
    size = int(n * (0.20 if len(label) > 3 else 0.24))
    font = ImageFont.truetype(FONT, size)
    box = d.textbbox((0, 0), label, font=font)
    d.text((cx - (box[2] - box[0]) / 2, n * 0.66), label, font=font,
           fill=(255, 255, 255, 235))

    img.resize((SIZE, SIZE), Image.LANCZOS).save(path)


if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    for name, (colour, label) in ICONS.items():
        out = os.path.join(here, "icons", f"{name}.png")
        os.makedirs(os.path.dirname(out), exist_ok=True)
        make(out, colour, label)
        print(f"{out}  {colour}  {label}")
