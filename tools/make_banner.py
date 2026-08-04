#!/usr/bin/env python3
"""Build the 720x320 appstore banner for TouchyWeather Face.

Mirrors the layout of the TouchyWeather app banner: hero icon at the left, a
row of watch bodies, callout labels with leader lines above, captions below,
and a tagline across the bottom.

The watch bodies and the hero icon are drawn here rather than sourced -- the
face repo ships only a 25x25 menu icon, which is far too small to scale up.
Screenshots are DOWNscaled to banner size, so they use LANCZOS; NEAREST is only
correct when upscaling pixel art by an integer factor.
"""
import os
from PIL import Image, ImageDraw, ImageFont

FACE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHOTS = f"{FACE}/store/screenshots/emery"
BRAND = f"{FACE}/resources/fonts/ChakraPetch-Bold.ttf"
BODY_B = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"

W, H = 720, 320
BG = (255, 255, 255)
INK = (17, 17, 17)
MUTED = (90, 96, 104)
LEADER = (150, 156, 164)
BODY_GREY = (154, 160, 166)
BODY_EDGE = (60, 64, 67)
BUTTON = (107, 112, 117)
SKY = (150, 205, 240)
SUN = (247, 168, 74)
CLOUD = (168, 174, 182)

ICON = 100
WATCH_W, WATCH_H = 124, 142


def f(path, size):
    return ImageFont.truetype(path, size)


def centre_text(dr, cx, y, text, font, fill):
    w = dr.textlength(text, font=font)
    dr.text((cx - w / 2, y), text, font=font, fill=fill)
    return w


def draw_icon(size):
    """Hero icon: sun + cloud over sky, with a clock face marking this as the
    watch FACE rather than the app (whose icon carries a touch cursor)."""
    S = 4
    d = size * S
    img = Image.new("RGB", (d, d), SKY)
    dr = ImageDraw.Draw(img)
    # sun
    cx, cy, r = d * 0.62, d * 0.33, d * 0.14
    for i in range(12):
        import math
        a = i * math.pi / 6
        dr.line([(cx + math.cos(a) * r * 1.35, cy + math.sin(a) * r * 1.35),
                 (cx + math.cos(a) * r * 2.0, cy + math.sin(a) * r * 2.0)],
                fill=SUN, width=int(d * 0.035))
    dr.ellipse([cx - r, cy - r, cx + r, cy + r], fill=SUN)
    # cloud
    dr.ellipse([d * 0.14, d * 0.36, d * 0.50, d * 0.66], fill=CLOUD)
    dr.ellipse([d * 0.34, d * 0.30, d * 0.72, d * 0.64], fill=CLOUD)
    dr.rounded_rectangle([d * 0.16, d * 0.50, d * 0.70, d * 0.66], radius=d * 0.08, fill=CLOUD)
    # clock face
    ccx, ccy, cr = d * 0.36, d * 0.70, d * 0.19
    dr.ellipse([ccx - cr, ccy - cr, ccx + cr, ccy + cr], fill=(255, 255, 255), outline=INK, width=int(d * 0.028))
    dr.line([(ccx, ccy), (ccx, ccy - cr * 0.58)], fill=INK, width=int(d * 0.030))
    dr.line([(ccx, ccy), (ccx + cr * 0.44, ccy + cr * 0.20)], fill=INK, width=int(d * 0.030))

    img = img.resize((size, size), Image.LANCZOS)
    # Corners are cut with alpha rather than filled white, so the same icon can
    # be composited onto any background (the banner pastes it onto white).
    mask = Image.new("L", (size * S, size * S), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, size * S - 1, size * S - 1], radius=size * S * 0.22, fill=255)
    mask = mask.resize((size, size), Image.LANCZOS)
    out = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    out.paste(img, (0, 0), mask)
    return out


def draw_watch(shot_path):
    """A stylised Pebble body with the screenshot inset."""
    S = 3
    w, h = WATCH_W * S, WATCH_H * S
    img = Image.new("RGB", (w, h), BG)
    dr = ImageDraw.Draw(img)
    lug = int(5 * S)
    # strap lugs
    for lx in (int(w * 0.16), int(w * 0.62)):
        dr.rounded_rectangle([lx, 0, lx + int(w * 0.22), lug * 3], radius=lug, fill=BUTTON)
        dr.rounded_rectangle([lx, h - lug * 3, lx + int(w * 0.22), h - 1], radius=lug, fill=BUTTON)
    # side buttons
    bw = int(4 * S)
    dr.rounded_rectangle([0, int(h * 0.42), bw * 2, int(h * 0.54)], radius=bw, fill=BUTTON)
    for by in (0.30, 0.56):
        dr.rounded_rectangle([w - bw * 2, int(h * by), w - 1, int(h * by) + int(h * 0.11)], radius=bw, fill=BUTTON)
    # body
    inset = int(2 * S)
    dr.rounded_rectangle([inset, lug, w - inset, h - lug], radius=int(11 * S), fill=BODY_GREY, outline=BODY_EDGE, width=S)
    # bezel -- kept thin: a heavier one shrinks the screen enough to cost
    # legibility on the denser pages at banner size
    bez = int(5 * S)
    dr.rounded_rectangle([inset + bez, lug + bez, w - inset - bez, h - lug - bez], radius=int(5 * S), fill=(15, 15, 15))

    sx0, sy0 = inset + bez + int(2 * S), lug + bez + int(2 * S)
    sx1, sy1 = w - inset - bez - int(2 * S), h - lug - bez - int(2 * S)
    sw, sh = sx1 - sx0, sy1 - sy0
    shot = Image.open(shot_path).convert("RGB")
    # preserve aspect, fit inside the screen well
    scale = min(sw / shot.width, sh / shot.height)
    nw, nh = int(shot.width * scale), int(shot.height * scale)
    shot = shot.resize((nw, nh), Image.LANCZOS)
    img.paste(shot, (sx0 + (sw - nw) // 2, sy0 + (sh - nh) // 2))

    return img.resize((WATCH_W, WATCH_H), Image.LANCZOS)


def main():
    canvas = Image.new("RGB", (W, H), BG)
    dr = ImageDraw.Draw(canvas)

    f_title = f(BRAND, 40)
    f_call = f(BODY_B, 10)
    f_cap = f(BODY_B, 10)
    f_tag = f(BRAND, 22)

    centre_text(dr, W / 2, 8, "TOUCHYWEATHER FACE", f_title, INK)

    watches = [
        (f"{SHOTS}/02-face-loaded.png", ["FOUR COMPLICATION", "SLOTS"], None),
        (f"{SHOTS}/08-peek-hours.png", ["NUDGE FOR THE", "FORECAST"], ["MEASURED FLOW", "LAYOUT"]),
        (f"{SHOTS}/07-night-mode.png", ["AUTOMATIC", "NIGHT MODE"], ["NO BUTTONS,", "NO TOUCH NEEDED"]),
        (f"{SHOTS}/12-overlay.png", ["EVERYTHING", "AT A GLANCE"], None),
    ]

    gap = 14
    row_w = ICON + 24 + len(watches) * WATCH_W + (len(watches) - 1) * gap
    x = (W - row_w) // 2
    watch_top = 106

    icon = draw_icon(ICON)
    canvas.paste(icon, (x, watch_top + (WATCH_H - ICON) // 2), icon)
    x += ICON + 24

    for path, caption, callout in watches:
        wimg = draw_watch(path)
        canvas.paste(wimg, (x, watch_top))
        cx = x + WATCH_W / 2

        if callout:
            cy = 62
            for i, line in enumerate(callout):
                centre_text(dr, cx, cy + i * 12, line, f_call, MUTED)
            dr.line([(cx, cy + len(callout) * 12 + 3), (cx, watch_top - 4)], fill=LEADER, width=1)

        cy = watch_top + WATCH_H + 7
        for i, line in enumerate(caption):
            centre_text(dr, cx, cy + i * 12, line, f_cap, INK)
        x += WATCH_W + gap

    centre_text(dr, W / 2, H - 34, "Weather, one flick of the wrist away.", f_tag, INK)

    out = f"{FACE}/store/banner.png"
    canvas.save(out)
    print(f"{out}  {canvas.size}  {os.path.getsize(out)//1024}KB")


if __name__ == "__main__":
    main()
