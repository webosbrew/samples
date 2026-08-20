#!/usr/bin/env python3
"""Generate the launcher icons: API name, play mark, variant.

The colour is not in the PNG - webOS paints it from appinfo's iconColor and bgColor, which
each sample sets to its own Material 500 value (the COLOR argument to webos_add_ipk). The
PNG carries only white artwork on transparency:

        SMP          <- which media API
         |>          <- play mark
         w4          <- which variant of it

Colour alone was not enough to tell eight tiles apart, and the two lines say which API and
which generation without having to remember a palette.

Run after changing the sample list; the PNGs are committed so the build needs no Python.
"""
import os
from PIL import Image, ImageDraw, ImageFont

FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
SIZE = 80
SS = 8  # supersample, for clean edges

# target name (minus the "media-" prefix) -> (API, variant)
ICONS = {
    "smp-acb-webos2":     ("SMP", "w2"),
    "smp-acb-webos3":     ("SMP", "w3"),
    "smp-acb-webos4":     ("SMP", "w4"),
    "smp-webos5":         ("SMP", "w5+"),
    "ndl-esplayer":       ("NDL", "ESP"),
    "ndl-directmedia-v1": ("NDL", "DM1"),
    "ndl-directmedia-v2": ("NDL", "DM2"),
    "lgnc":               ("LGNC", "1-4"),
}


def centred(d, text, font, cy, n):
    box = d.textbbox((0, 0), text, font=font)
    d.text((n / 2 - (box[2] - box[0]) / 2, cy - (box[3] - box[1]) / 2 - box[1]),
           text, font=font, fill=(255, 255, 255, 255))


def make(path, api, variant):
    n = SIZE * SS
    img = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)

    # Longer words get a smaller face so "LGNC" fits the same box as "SMP".
    centred(d, api, ImageFont.truetype(FONT, int(n * (0.19 if len(api) > 3 else 0.23))),
            n * 0.16, n)

    cx, cy = n * 0.53, n * 0.50
    h = n * 0.30
    w = h * 0.87
    d.polygon([(cx - w / 2, cy - h / 2), (cx - w / 2, cy + h / 2), (cx + w / 2, cy)],
              fill=(255, 255, 255, 255))

    centred(d, variant, ImageFont.truetype(FONT, int(n * 0.21)), n * 0.85, n)

    img.resize((SIZE, SIZE), Image.LANCZOS).save(path)


if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    out_dir = os.path.join(here, "icons")
    os.makedirs(out_dir, exist_ok=True)
    for name, (api, variant) in ICONS.items():
        out = os.path.join(out_dir, f"{name}.png")
        make(out, api, variant)
        print(f"{out}  {api}/{variant}")
