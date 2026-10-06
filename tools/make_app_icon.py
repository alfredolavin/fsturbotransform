#!/usr/bin/env python3
"""Builds the app icon assets from a transparent master PNG (e.g. from Nano Banana).

  assets/icon.png      512x512 transparent PNG (README / desktop use)
  src/app_icon.rgba    128x128 straight-alpha RGBA, #embed-ed by src/app_icon.hpp

Usage: tools/make_app_icon.py MASTER.png
"""
import sys
from PIL import Image

EMBED_SIZE = 128  # keep in sync with kAppIconSize in src/app_icon.hpp
PNG_SIZE = 512


def square_crop(im: Image.Image, margin: float = 0.04) -> Image.Image:
    """Crops to the visible content (alpha > 6) and pads to a centered square."""
    box = im.getchannel("A").point(lambda a: 255 if a > 6 else 0).getbbox()
    im = im.crop(box)
    side = round(max(im.size) * (1 + 2 * margin))
    out = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    out.paste(im, ((side - im.width) // 2, (side - im.height) // 2))
    return out


def resize_premultiplied(im: Image.Image, size: int) -> Image.Image:
    """Lanczos resize in premultiplied alpha so transparent pixels never bleed dark fringes."""
    pm = im.convert("RGBa").resize((size, size), Image.LANCZOS)
    return pm.convert("RGBA")


def main() -> None:
    master = square_crop(Image.open(sys.argv[1]).convert("RGBA"))
    resize_premultiplied(master, PNG_SIZE).save("assets/icon.png", optimize=True)
    with open("src/app_icon.rgba", "wb") as f:
        f.write(resize_premultiplied(master, EMBED_SIZE).tobytes())


if __name__ == "__main__":
    main()
