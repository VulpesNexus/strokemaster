"""Sets each switch's example off and on side by side (tools/jsx/build-feature-examples.jsx,
run by tools/run-id-examples.ps1), as images that show what the switch tackles.

Each cell of feature-examples.json is rendered from the export with its switch off (on by
default: fx-off-<key>.pdf; off by default: fx-defaults.pdf) and with it on (fx-defaults.pdf or
fx-on-<key>.pdf), cropped to the ink of both, and placed off on the left, on on the right. Where
the two differ, rings mark the place on both sides, so the eye goes to what the switch changes.

Two sets: --repo writes wide images with a title and a sentence each, for the documentation;
--social writes 1600 x 900 images (16:9, shown uncropped in a timeline) with large type and a
short line, readable on a phone.

usage: compose_feature_examples.py <examples folder> --repo DIR --social DIR
"""
import argparse
import json
import os
from pathlib import Path

import numpy as np
import pymupdf
from PIL import Image, ImageDraw, ImageFont
from scipy import ndimage

# key: (title, what InDesign does without the switch, a short line for the social images, apps)
FEATURES = {
    'shapes': ("Shapes, frames, and outlined text",
               "InDesign's outward offset breaks on script outlines: openings flood and edges inside the letters are stroked.",
               "Outside strokes on script outlines: no flooded openings", 'InDesign'),
    'inside': ("Inside strokes",
               "InDesign's inward offset drops the thin parts of a letter or spills past them.",
               "Inside strokes that keep thin parts", 'InDesign'),
    'open_paths': ("Open paths",
                   "Past the miter limit, InDesign cuts an open path's sharp points flat; closed shapes keep them whole.",
                   "Whole points on open paths", 'InDesign'),
    'transparency': ("Objects with opacity or effects",
                     "A translucent stroke is stroked once per contour, so it darkens where contours overlap.",
                     "Translucent strokes drawn as one piece", 'InDesign'),
    'text_behind': ("Strokes behind other letters' fills",
                    "Each line's stroke covers the line above, and a recolored run's stroke covers the letter before it.",
                    "Text strokes beneath every letter", 'InDesign and Illustrator'),
    'text_story': ("Across a story's threaded frames",
                   "The stroke of a threaded frame covers the letters of the frame before it.",
                   "Strokes beneath letters across threaded frames", 'InDesign and Illustrator'),
    'text_stories': ("Across separate stories",
                     "One story's stroke covers the letters of another story it overlaps. Off by default.",
                     "Strokes beneath letters across separate stories", 'InDesign and Illustrator'),
    'text_miter': ("No miter spikes in live text",
                   "Miters at the inner corners of letters poke out through their thin parts as spikes.",
                   "No miter spikes in live text", 'InDesign and Illustrator'),
    'shape_miter': ("No miter spikes in shapes and outlines",
                    "Miters at inner corners poke out through the thin parts of shapes and outlined text.",
                    "No miter spikes in shapes and outlines", 'InDesign and Illustrator'),
    'glyph_tips': ("Trim glyph tips",
                   "The sharp tips of letters end in long miter points.",
                   "Letter tips cut flush, not spiked", 'InDesign and Illustrator'),
    'whole_points': ("Whole points past the miter limit",
                     "Past the miter limit, a shape's own sharp points are cut flat at their tips.",
                     "Sharp points kept whole", 'InDesign and Illustrator'),
    'pattern_points': ("Also on dashed and striped strokes",
                       "Dashed, dotted, and striped strokes lose their points to the pattern's bevel.",
                       "Whole points on dashed and striped strokes", 'InDesign and Illustrator'),
    'effect_corners': ("Trim corner-effect corners",
                       "A corner effect's small zigzags grow long miter points. Shown with Trim glyph tips off, which otherwise trims most of them.",
                       "Corner effects without long points (Trim glyph tips off)", 'InDesign'),
    'overlaps': ("Overlapping contours get one outline",
                 "Overlapping contours are each stroked, so strokes run along edges inside the shape.",
                 "One outline around overlapping shapes", 'InDesign and Illustrator'),
    'word_outline': ("One outline per word (dashes, stripes)",
                     "Dashes follow each contour, so they cross where letters and shapes overlap.",
                     "Dashes along one outline per word", 'InDesign'),
    'close_holes': ("Close holes inside the outline",
                    "Counters, and the gaps that touching letters' strokes close off, show what lies beneath. Off by default.",
                    "Holes inside the outline closed", 'InDesign and Illustrator'),
}

# Switches whose effect alone is small next to StrokeMaster's other corrections: shown against
# InDesign alone (fx-none.pdf) instead of the switch off, and labeled so.
AGAINST_INDESIGN = set()
# Examples whose change is small: the part shown, in points from the cell's left and top.
ZOOM = {}
# Examples plain enough that rings would only clutter them.
NO_RINGS = {'close_holes'}

for _k, _v in FEATURES.items():   # true apostrophes on the images and the page
    FEATURES[_k] = tuple(s.replace("'", '’') for s in _v)

INK = 245          # a pixel darker than this in any channel is ink
RING = (214, 32, 120)
TEXT = (28, 32, 44)
MUTED = (96, 104, 120)


def fonts_dir():
    # Windows' own folder, from its environment (SystemRoot names it too).
    return Path(os.environ.get('WINDIR') or os.environ['SYSTEMROOT']) / 'Fonts'


def font(size, bold=False):
    name = 'segoeuib.ttf' if bold else 'segoeui.ttf'
    try:
        return ImageFont.truetype(str(fonts_dir() / name), size)
    except OSError:
        return ImageFont.load_default()


def render(pdf, clip, dpi):
    page = pymupdf.open(pdf)[0]
    pix = page.get_pixmap(dpi=dpi, clip=clip, alpha=False)
    return np.frombuffer(pix.samples, np.uint8).reshape(pix.h, pix.w, pix.n)[..., :3].copy()


def ink_box(*images):
    mask = np.zeros(images[0].shape[:2], bool)
    for im in images:
        mask |= (im < INK).any(axis=2)
    ys, xs = np.nonzero(mask)
    return xs.min(), ys.min(), xs.max() + 1, ys.max() + 1


def rings(a, b, scale):
    """Circles around the places where the two renders differ, merged when close: none when the
    change is everywhere (more than six places), where rings would only add noise."""
    diff = (np.abs(a.astype(int) - b.astype(int)).max(axis=2) > 60)
    diff = ndimage.binary_opening(diff, iterations=2)
    diff = ndimage.binary_dilation(diff, iterations=max(3, int(22 * scale)))
    labels, n = ndimage.label(diff)
    out = []
    for sl in ndimage.find_objects(labels):
        if sl is None:
            continue
        y0, y1, x0, x1 = sl[0].start, sl[0].stop, sl[1].start, sl[1].stop
        if (x1 - x0) * (y1 - y0) < (50 * scale) ** 2:
            continue
        if max(x1 - x0, y1 - y0) > 0.4 * min(a.shape[:2]):
            continue   # a ring around most of the picture says nothing
        out.append((x0, y0, x1, y1))
    return out if len(out) <= 6 else []


def cell_images(folder, cell, dpi):
    key, on = cell['key'], cell['on']
    off_pdf = folder / ('fx-none.pdf' if key in AGAINST_INDESIGN else f'fx-off-{key}.pdf' if on else 'fx-defaults.pdf')
    on_pdf = folder / ('fx-defaults.pdf' if on else f'fx-on-{key}.pdf')
    if cell.get('base'):
        on_pdf = folder / f'fx-base-{key}.pdf'   # the same base switches, this one at its default
    top = cell['top']
    clip = pymupdf.Rect(0, top - 25, 340, top + cell['h'] + 25)
    if key in ZOOM:
        x0, y0, x1, y1 = ZOOM[key]
        clip = pymupdf.Rect(x0, top + y0, x1, top + y1)
        dpi = dpi * 3
    a, b = render(off_pdf, clip, dpi), render(on_pdf, clip, dpi)
    x0, y0, x1, y1 = ink_box(a, b)
    pad = int(dpi / 72 * 8)
    x0, y0 = max(0, x0 - pad), max(0, y0 - pad)
    x1, y1 = min(a.shape[1], x1 + pad), min(a.shape[0], y1 + pad)
    return a[y0:y1, x0:x1], b[y0:y1, x0:x1]


def panel(img, marks, width, height, label, chip_font, bg=(255, 255, 255)):
    """The render fitted into width x height, its rings drawn, a chip with `label` at top left."""
    h, w = img.shape[:2]
    s = min(width / w, (height - chip_font.size * 2) / h)
    im = Image.fromarray(img).resize((max(1, int(w * s)), max(1, int(h * s))), Image.LANCZOS)
    canvas = Image.new('RGB', (width, height), bg)
    ox, oy = (width - im.width) // 2, chip_font.size * 2 + (height - chip_font.size * 2 - im.height) // 2
    canvas.paste(im, (ox, oy))
    d = ImageDraw.Draw(canvas)
    lw = max(3, width // 260)
    for (x0, y0, x1, y1) in marks:
        cx, cy = ox + (x0 + x1) / 2 * s, oy + (y0 + y1) / 2 * s
        r = max((x1 - x0), (y1 - y0)) / 2 * s * 1.15 + lw * 2
        d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=RING, width=lw)
    tw = d.textlength(label, font=chip_font)
    pad = chip_font.size // 2
    on = label in ('On', 'StrokeMaster')
    d.rounded_rectangle([0, 0, tw + 2 * pad, chip_font.size + pad * 2], radius=pad,
                        fill=(30, 120, 80) if on else (120, 128, 140))
    d.text((pad, pad - chip_font.size // 8), label, fill=(255, 255, 255), font=chip_font)
    return canvas


def wrap(d, text, f, width):
    words, lines, cur = text.split(), [], ''
    for w in words:
        t = (cur + ' ' + w).strip()
        if d.textlength(t, font=f) <= width:
            cur = t
        else:
            lines.append(cur)
            cur = w
    if cur:
        lines.append(cur)
    return lines


def labels(key):
    return ('InDesign alone', 'StrokeMaster') if key in AGAINST_INDESIGN else ('Off', 'On')


def repo_image(key, a, b, marks, path):
    title, what, _, apps = FEATURES[key]
    W, M, G = 1600, 56, 40
    pw = (W - 2 * M - G) // 2
    ph = 620
    tf, bf, cf, sf = font(48, True), font(30), font(30, True), font(22)
    probe = ImageDraw.Draw(Image.new('RGB', (W, 10)))
    lines = wrap(probe, what, bf, W - 2 * M)
    head = M + 60 + len(lines) * 42 + 30
    img = Image.new('RGB', (W, head + ph + 70), (255, 255, 255))
    d = ImageDraw.Draw(img)
    d.text((M, M - 10), title, fill=TEXT, font=tf)
    for i, line in enumerate(lines):
        d.text((M, M + 62 + i * 42), line, fill=MUTED, font=bf)
    off, on = labels(key)
    img.paste(panel(a, marks, pw, ph, off, cf), (M, head))
    img.paste(panel(b, marks, pw, ph, on, cf), (M + pw + G, head))
    d.line([(M + pw + G // 2, head), (M + pw + G // 2, head + ph)], fill=(225, 228, 234), width=2)
    d.text((M, head + ph + 24), f'StrokeMaster: {apps}. Rendered by InDesign 2026; rings mark what changes.',
           fill=MUTED, font=sf)
    img.save(path, optimize=True)


def social_image(key, a, b, marks, path):
    title, _, short, _ = FEATURES[key]
    W, H, M, G = 1600, 900, 48, 32
    tf, sf, cf = font(66, True), font(44), font(44, True)
    img = Image.new('RGB', (W, H), (255, 255, 255))
    d = ImageDraw.Draw(img)
    size = 66
    while d.textlength(title, font=tf) > W - 2 * M and size > 40:
        size -= 2
        tf = font(size, True)
    d.text((M, M - 14), title, fill=TEXT, font=tf)
    ssize = 44
    while d.textlength(short, font=sf) > W - 2 * M and ssize > 30:
        ssize -= 2
        sf = font(ssize)
    d.text((M, M + size + 4), short, fill=MUTED, font=sf)
    top = M + size + ssize + 40
    pw, ph = (W - 2 * M - G) // 2, H - top - M + 10
    off, on = labels(key)
    img.paste(panel(a, marks, pw, ph, off, cf), (M, top))
    img.paste(panel(b, marks, pw, ph, on, cf), (M + pw + G, top))
    d.line([(M + pw + G // 2, top + 10), (M + pw + G // 2, H - M)], fill=(225, 228, 234), width=3)
    img.save(path, optimize=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('folder')
    ap.add_argument('--repo', required=True)
    ap.add_argument('--social', required=True)
    ap.add_argument('--dpi', type=int, default=600)
    args = ap.parse_args()
    folder = Path(args.folder)
    meta = json.loads((folder / 'feature-examples.json').read_text(encoding='utf-8-sig'))
    Path(args.repo).mkdir(parents=True, exist_ok=True)
    Path(args.social).mkdir(parents=True, exist_ok=True)
    for cell in meta['cells']:
        key = cell['key']
        a, b = cell_images(folder, cell, args.dpi)
        marks = [] if key in NO_RINGS else rings(a, b, args.dpi * (3 if key in ZOOM else 1) / 300)
        repo_image(key, a, b, marks, Path(args.repo) / f'{key}.png')
        social_image(key, a, b, marks, Path(args.social) / f'{key}.png')
        print(key, a.shape[1], a.shape[0], 'rings', len(marks))


if __name__ == '__main__':
    main()
