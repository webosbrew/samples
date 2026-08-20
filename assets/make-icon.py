#!/usr/bin/env python3
"""Generate the launcher icon: a play mark on transparency, nothing else.

The tile's colour does not belong in the PNG - webOS paints it from appinfo's iconColor and
bgColor, which each sample sets to its own Material 500 value (see the COLOR argument to
webos_add_ipk). So one icon serves all the samples and they still look different on the
home screen.
"""
import os
from PIL import Image, ImageDraw

SIZE = 80
SS = 8  # supersample, for a clean triangle edge


def main():
    n = SIZE * SS
    img = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)

    cx, cy = n * 0.54, n * 0.5  # nudged right so the triangle looks centred
    h = n * 0.46
    w = h * 0.87
    d.polygon([(cx - w / 2, cy - h / 2), (cx - w / 2, cy + h / 2), (cx + w / 2, cy)],
              fill=(255, 255, 255, 255))

    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "icon.png")
    img.resize((SIZE, SIZE), Image.LANCZOS).save(out)
    print(out)


if __name__ == "__main__":
    main()
