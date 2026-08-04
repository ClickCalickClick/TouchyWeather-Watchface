#!/usr/bin/env python3
"""Export the TouchyWeather Face app icon at the appstore sizes.

Shares draw_icon() with make_banner.py so the banner's hero icon and the
shipped icons can never drift apart. Output is RGBA with the rounded corners
cut as alpha, so it composites onto any background.

Each size is drawn at its own supersampled resolution rather than being
downscaled from one master -- the stroke widths are proportional, so drawing at
the target size keeps the clock hands and sun rays from thinning out at 80px.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_banner import draw_icon, FACE  # noqa: E402

SIZES = [80, 144]


def main():
    out_dir = f"{FACE}/store/icons"
    os.makedirs(out_dir, exist_ok=True)
    for s in SIZES:
        icon = draw_icon(s)
        path = f"{out_dir}/icon-{s}.png"
        icon.save(path)
        print(f"{path}  {icon.size}  {icon.mode}  {os.path.getsize(path)}B")


if __name__ == "__main__":
    main()
