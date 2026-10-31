"""Compose an Illustrator showcase from a certification run: before and after, side by side.

    python tools/compose_illustrator_showcase.py build/ai-cert-<n> --out build/look/<folder>/showcase.pdf

Each page shows one job's native export beside its corrected export (and, for the re-fix job,
the export after the user's recolor), rendered by MuPDF at 110 ppi and labeled with the
certification's numbers. Review material: it stays in the ignored build folder.
"""
import argparse
import json
from pathlib import Path

import pymupdf
from PIL import Image, ImageDraw, ImageFont

PAGES = [
    ("story-minion-rgb", "Threaded story, one fill, mixed strokes: one group fill above the text"),
    ("story-minion-mixfill", "Threaded story, mixed fills, one stroke: one group stroke below the text"),
    ("story-minion-separate", "Two separate stories, opt-in: one story's strokes beneath the other's fills"),
    ("story-minion-perframe", "The same story routed frame by frame: a later frame still covers an earlier one"),
    ("story-kerber-uniform", "Kerberos Fang, one fill and one stroke"),
    ("story-german-uniform", "German Beauty, one fill and one stroke"),
    ("story-greatv-rgb", "Great Vibes, one fill, mixed strokes"),
    ("runs-split4", "Four frames with a rectangle stacked between frames 2 and 3: two runs"),
    ("refix", "Re-fix after a recolor (A story fill, B story stroke, C frame stroke, D frame fill, E recolored whole)"),
]


def render(path, dpi=110):
    pix = pymupdf.open(path)[0].get_pixmap(dpi=dpi, alpha=False)
    return Image.frombytes("RGB", (pix.width, pix.height), pix.samples)


def trim(images, pad=20):
    # One crop box for all, from the union of their ink.
    box = None
    for im in images:
        b = Image.eval(im.convert("L"), lambda v: 255 - v).getbbox()
        if b:
            box = b if box is None else (min(box[0], b[0]), min(box[1], b[1]), max(box[2], b[2]), max(box[3], b[3]))
    if box is None:
        return images
    w, h = images[0].size
    box = (max(0, box[0] - pad), max(0, box[1] - pad), min(w, box[2] + pad), min(h, box[3] + pad))
    return [im.crop(box) for im in images]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("run", type=Path)
    ap.add_argument("--out", type=Path, required=True)
    args = ap.parse_args()
    cert = json.loads((args.run / "certification.json").read_text(encoding="utf-8"))
    try:
        font = ImageFont.truetype("segoeui.ttf", 22)
        small = ImageFont.truetype("segoeui.ttf", 17)
    except OSError:
        font = small = ImageFont.load_default()
    pages = []
    for job, title in PAGES:
        folder = args.run / job
        if not folder.exists():
            continue
        m = cert["jobs"].get(job, {})
        if job == "refix":
            names = [("Native", f"{job}-native.pdf"), ("Corrected", f"{job}-route.pdf"), ("After the recolor and re-fix", f"{job}-recolor.pdf")]
            note = "After the recolor, pt2: " + ", ".join(f"{k} {m[k]['recolor']}" for k in "ABCDE" if k in m) + f"; text {m.get('text')}"
        else:
            stem = job + ("-" + job.split("-", 2)[-1] if job.startswith("story") else "-" + job.split("-", 1)[-1])
            fixed = folder / f"{stem}-fixed.pdf"
            names = [("Native", f"{stem}-native.pdf"),
                     ("Corrected (route and native-fix)" if fixed.exists() else "Corrected", fixed.name if fixed.exists() else f"{stem}-route.pdf")]
            note = f"Fill damage {m.get('native')} -> {m.get('damage')} pt2; text extracted {m.get('text')}; route {m.get('gate')}"
        images = trim([render(folder / n) for _, n in names])
        w = sum(im.width for im in images) + 40 * (len(images) + 1)
        h = max(im.height for im in images) + 150
        page = Image.new("RGB", (max(w, 900), h), "white")
        d = ImageDraw.Draw(page)
        d.text((40, 20), title, fill="black", font=font)
        d.text((40, h - 45), note, fill=(80, 80, 80), font=small)
        x = 40
        for (label, _), im in zip(names, images):
            d.text((x, 62), label, fill=(80, 80, 80), font=small)
            page.paste(im, (x, 90))
            x += im.width + 40
        pages.append(page)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    pages[0].save(args.out, save_all=True, append_images=pages[1:], resolution=110)
    print(f"{len(pages)} pages: {args.out}")


if __name__ == "__main__":
    main()
