#!/usr/bin/env python3
"""Generate the launcher icons: API name, play mark, variant.

The colour is not in the PNG - webOS paints it from appinfo's iconColor and bgColor, which
each sample sets to its own Material 500 value (the COLOR argument to webos_add_ipk). The
PNG carries only white artwork on transparency:

        SMP          <- which API
         |>          <- play mark, or a globe for the web samples
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

# target name (minus the "media-" prefix) -> (API, variant[, mark])
ICONS = {
    "smp-acb-webos2":     ("SMP", "w2"),
    "smp-acb-webos3":     ("SMP", "w3"),
    "smp-acb-webos4":     ("SMP", "w4"),
    "smp-webos5":         ("SMP", "w5+"),
    "ndl-esplayer":       ("NDL", "ESP"),
    "ndl-directmedia-v1": ("NDL", "DM1"),
    "ndl-directmedia-v2": ("NDL", "DM2"),
    "lgnc":               ("LGNC", "1-4"),
    "web-cbe":            ("CBE", "w4", "globe"),
}


def centred(d, text, font, cy, n):
    box = d.textbbox((0, 0), text, font=font)
    d.text((n / 2 - (box[2] - box[0]) / 2, cy - (box[3] - box[1]) / 2 - box[1]),
           text, font=font, fill=(255, 255, 255, 255))


def play_mark(d, cx, cy, h):
    w = h * 0.87
    d.polygon([(cx - w / 2, cy - h / 2), (cx - w / 2, cy + h / 2), (cx + w / 2, cy)],
              fill=(255, 255, 255, 255))


def globe_mark(d, cx, cy, h):
    """A ring with one meridian and one parallel - enough to read as "web" at 80px."""
    r = h / 2
    t = max(2, int(h * 0.075))
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=(255, 255, 255, 255), width=t)
    d.ellipse([cx - r * 0.45, cy - r, cx + r * 0.45, cy + r],
              outline=(255, 255, 255, 255), width=t)
    d.line([cx - r, cy, cx + r, cy], fill=(255, 255, 255, 255), width=t)


MARKS = {"play": play_mark, "globe": globe_mark}


def make(path, api, variant, mark="play"):
    n = SIZE * SS
    img = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)

    # Longer words get a smaller face so "LGNC" fits the same box as "SMP".
    centred(d, api, ImageFont.truetype(FONT, int(n * (0.19 if len(api) > 3 else 0.23))),
            n * 0.16, n)

    MARKS[mark](d, n * 0.53 if mark == "play" else n * 0.50, n * 0.50, n * 0.30)

    centred(d, variant, ImageFont.truetype(FONT, int(n * 0.21)), n * 0.85, n)

    img.resize((SIZE, SIZE), Image.LANCZOS).save(path)


if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    out_dir = os.path.join(here, "icons")
    os.makedirs(out_dir, exist_ok=True)
    for name, spec in ICONS.items():
        out = os.path.join(out_dir, f"{name}.png")
        make(out, *spec)
        print(f"{out}  {spec[0]}/{spec[1]}")
