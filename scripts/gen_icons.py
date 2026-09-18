#!/usr/bin/env python3
"""Generate every launcher / store icon from one vector description.

The icons are the game itself in miniature: a two-by-two patch of the grid, drawn
the way src/render.c draws it -- a flag on a hidden cell, a 1 and a 2 on
revealed cells, and a mine on the red of a detonated cell. Keeping them
generated rather than hand-drawn means the palette can never drift from
src/render.c, and every size is produced from the same geometry.

Outputs (run from the repo root, needs Pillow):
    android/res/mipmap-*/ic_launcher.png            legacy square launcher icon
    android/res/mipmap-*/ic_launcher_foreground.png adaptive-icon foreground
    android/play-assets/icon-512.png                Play store listing icon
    android/play-assets/feature-graphic-1024x500.png Play store feature graphic
    ios/Assets.xcassets/AppIcon.appiconset/icon-1024.png  iOS app icon
    ios/app-store-assets/icon-1024.png              App Store listing icon

    scripts/gen_icons.py
"""
import os

from PIL import Image, ImageDraw, ImageFont

# Palette, copied from the constants at the top of src/render.c.
BOARD = (30, 30, 30, 255)          # COL_TOPBAR: the icon tile
BOARD_DARK = (20, 20, 20, 255)     # COL_BG
CELL_HIDDEN = (35, 35, 35, 255)    # COL_CELL_UNREV, lifted a touch to read at 48px
CELL_OPEN = (20, 20, 20, 255)      # COL_CELL_REV
BORDER = (60, 60, 60, 255)         # COL_BORDER
MINE_BG = (220, 60, 60, 255)       # COL_MINE_BG
MINE = (200, 200, 200, 255)        # COL_MINE
HIGHLIGHT = (220, 220, 220, 255)   # COL_WHITE
FLAG = (240, 200, 40, 255)         # COL_FLAG
NUM1 = (100, 120, 240, 255)        # NUM_COLORS[1]
NUM2 = (60, 180, 60, 255)          # NUM_COLORS[2]

FONT = "third_party/fonts/nunito/Nunito-SemiBold.ttf"
SS = 4  # supersample factor; every shape is drawn large and downscaled


def number(d, x, y, s, text, colour):
    font = ImageFont.truetype(FONT, int(s * 0.78))
    box = d.textbbox((0, 0), text, font=font)
    tw, th = box[2] - box[0], box[3] - box[1]
    d.text((x + (s - tw) / 2 - box[0], y + (s - th) / 2 - box[1]), text, font=font, fill=colour)


def mine(d, x, y, s):
    cx, cy = x + s / 2, y + s / 2
    arm, t = s * 0.78, max(2, s * 0.13)
    d.rectangle([cx - t / 2, cy - arm / 2, cx + t / 2, cy + arm / 2], fill=MINE)
    d.rectangle([cx - arm / 2, cy - t / 2, cx + arm / 2, cy + t / 2], fill=MINE)
    r = s * 0.28
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=MINE)
    hr = s * 0.08
    hx, hy = cx - s * 0.09, cy - s * 0.09
    d.ellipse([hx - hr, hy - hr, hx + hr, hy + hr], fill=HIGHLIGHT)


def flag(d, x, y, s):
    pole_x, pole_w = x + s * 0.55, max(2, s * 0.07)
    d.rectangle([pole_x, y + s * 0.16, pole_x + pole_w, y + s * 0.84], fill=MINE)
    d.rectangle([x + s * 0.30, y + s * 0.80, x + s * 0.70, y + s * 0.88], fill=MINE)
    d.polygon([(pole_x, y + s * 0.16), (pole_x, y + s * 0.50), (x + s * 0.20, y + s * 0.33)],
              fill=FLAG)


def patch(size):
    """The 2x2 grid patch, `size` px square, transparent outside the cells."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    s = size // 2
    line = max(2, size // 80)
    cells = [((0, 0), CELL_HIDDEN), ((s, 0), CELL_OPEN), ((0, s), CELL_OPEN), ((s, s), MINE_BG)]
    for (x, y), fill in cells:
        d.rectangle([x, y, x + s - 1, y + s - 1], fill=fill, outline=BORDER, width=line)
    flag(d, 0, 0, s)
    number(d, s, 0, s, "1", NUM1)
    number(d, 0, s, s, "2", NUM2)
    mine(d, s, s, s)
    return img


def compose(size, background, content_scale):
    """The icon at `size` px. `background` is None for the adaptive foreground.

    `content_scale` is the fraction of the canvas the grid patch spans, so the
    adaptive foreground can stay inside its 66% safe zone while the legacy
    square icon fills more of its tile.
    """
    n = size * SS
    img = Image.new("RGBA", (n, n), background if background else (0, 0, 0, 0))
    span = int(n * content_scale)
    art = patch(span)
    img.alpha_composite(art, ((n - span) // 2, (n - span) // 2))
    return img.resize((size, size), Image.LANCZOS)


def feature_graphic(w, h):
    """Play's 1024x500 feature graphic: the icon art on a board gradient."""
    img = Image.new("RGBA", (w * 2, h * 2), BOARD)
    d = ImageDraw.Draw(img)
    for y in range(h * 2):  # subtle vertical shade, dark at the bottom
        t = y / (h * 2)
        d.line([0, y, w * 2, y],
               fill=tuple(int(BOARD[i] + (BOARD_DARK[i] - BOARD[i]) * t) for i in range(3)))
    art = compose(h * 2, None, 0.66)
    img.alpha_composite(art, ((w * 2 - art.width) // 2, 0))
    return img.resize((w, h), Image.LANCZOS)


def save(img, path, opaque=False):
    """Write `img`, flattening away the alpha channel when `opaque` is set.

    Apple rejects an app icon that has an alpha channel outright -- it masks the
    corners itself -- so the iOS icons must be flat RGB. The Android adaptive
    foreground is the opposite case and must keep its transparency.
    """
    os.makedirs(os.path.dirname(path), exist_ok=True)
    if opaque:
        flat = Image.new("RGB", img.size, BOARD[:3])
        flat.paste(img, mask=img.split()[3])
        img = flat
    img.save(path)
    print("gen_icons: wrote %s (%dx%d %s)" % (path, img.width, img.height, img.mode))


def main():
    # Legacy square launcher icon and the adaptive foreground, per density. The
    # adaptive foreground canvas is 108dp to the legacy 48dp, and its content
    # must stay inside the central 72dp, hence the smaller content scale.
    for suffix, legacy in (("mdpi", 48), ("hdpi", 72), ("xhdpi", 96),
                           ("xxhdpi", 144), ("xxxhdpi", 192)):
        d = "android/res/mipmap-%s" % suffix
        save(compose(legacy, BOARD, 0.78), "%s/ic_launcher.png" % d)
        save(compose(legacy * 108 // 48, None, 0.55),
             "%s/ic_launcher_foreground.png" % d)

    save(compose(512, BOARD, 0.72), "android/play-assets/icon-512.png")
    save(feature_graphic(1024, 500), "android/play-assets/feature-graphic-1024x500.png",
         opaque=True)

    ios_icon = compose(1024, BOARD, 0.72)
    save(ios_icon, "ios/Assets.xcassets/AppIcon.appiconset/icon-1024.png", opaque=True)
    save(ios_icon, "ios/app-store-assets/icon-1024.png", opaque=True)


if __name__ == "__main__":
    main()
