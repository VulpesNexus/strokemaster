"""Measure the Illustrator text-gaps probe (tools/jsx/ai-text-gaps.jsx), without Adobe.

usage: measure_text_gaps.py <folder> <runId> [--out JSON]

Part A: the mixed-stroke fixture under native, the native route, and the text-kept effect,
in each export format. PDF and SVG are rendered by MuPDF, EPS by Ghostscript, PNG as
exported (600 ppi). For each format the fill reference (strokes removed) gives the glyph
fill mask, eroded by 3 px at 600 ppi into the fill core; a core pixel lighter than level 32
is damage (a stroke painted over a fill). Text kept: the characters MuPDF extracts (PDF,
SVG) or the count of text-showing operators (EPS: Tj/TJ/show in the PostScript). EPS files are
exported cropped to the artboard (named <name>-01.eps), so they share one origin. Part B
(threaded) and C (existing appearance) are measured as PDFs the same way.
"""
import argparse
import json
import re
import shutil
import subprocess
import tempfile
from pathlib import Path

import numpy
import pymupdf
from PIL import Image
from scipy import ndimage

DPI = 600
ERODE = 3
DARK = 32
GS = [r'C:\Program Files\gs\gs9.23\bin\gswin64c.exe', r'C:\Program Files\gs\gs10.04.0\bin\gswin64c.exe']


def gray_pdf_svg(path):
    with pymupdf.open(path) as doc:
        page = doc[0]
        pix = page.get_pixmap(dpi=DPI, colorspace=pymupdf.csGRAY, alpha=False)
        img = numpy.frombuffer(pix.samples, numpy.uint8).reshape(pix.height, pix.width).copy()
        return img, len(page.get_text().strip().replace('\n', ''))


def gray_eps(path):
    gs = next((g for g in GS if Path(g).is_file()), None)
    if not gs:
        return None, None
    with tempfile.TemporaryDirectory() as tmp:
        src = Path(tmp) / 'in.eps'
        shutil.copyfile(path, src)   # Ghostscript cannot open the workspace's en dash
        png = Path(tmp) / 'out.png'
        subprocess.run([gs, '-q', '-dNOPAUSE', '-dBATCH', '-dSAFER', '-dEPSCrop', '-sDEVICE=pnggray',
                        f'-r{DPI}', '-dTextAlphaBits=4', '-dGraphicsAlphaBits=4', f'-sOutputFile={png}', str(src)],
                       check=True, capture_output=True)
        img = numpy.array(Image.open(png).convert('L'))
    text = path.read_bytes().decode('latin-1')
    shows = len(re.findall(r'\b(?:Tj|TJ|show|ashow|widthshow|xshow|xyshow)\b', text))
    return img, shows


def gray_png(path):
    return numpy.array(Image.open(path).convert('L')), None


def load(path):
    ext = path.suffix.lower()
    if not path.exists() and ext == '.eps':
        path = path.with_name(path.stem + '-01.eps')   # an artboard EPS is named with its number
    if ext in ('.pdf', '.svg'):
        return gray_pdf_svg(path)
    if ext == '.eps':
        return gray_eps(path)
    return gray_png(path)


def damage(fill_img, img):
    if fill_img is None or img is None:
        return None
    h, w = min(fill_img.shape[0], img.shape[0]), min(fill_img.shape[1], img.shape[1])
    mask = fill_img[:h, :w] < DARK
    core = ndimage.binary_erosion(mask, iterations=ERODE)
    return {'core_px': int(core.sum()), 'core_damaged_px': int((core & (img[:h, :w] >= DARK)).sum())}


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('folder', type=Path)
    ap.add_argument('run')
    ap.add_argument('--out', type=Path)
    a = ap.parse_args()
    res = {'dpi': DPI, 'erode_px': ERODE, 'dark_level': DARK, 'A': {}, 'B': {}, 'C': {}}
    for ext in ('pdf', 'svg', 'eps', 'png'):
        fill, _ = load(a.folder / f'{a.run}-A-fill.{ext}')
        for case in ('native', 'route', 'effect'):
            img, text = load(a.folder / f'{a.run}-A-{case}.{ext}')
            r = damage(fill, img) or {}
            r['text'] = text
            res['A'][f'{case}.{ext}'] = r
    fill, _ = load(a.folder / f'{a.run}-B-fill.pdf')
    for case in ('native', 'route', 'effect'):
        img, text = load(a.folder / f'{a.run}-B-{case}.pdf')
        res['B'][case] = dict(damage(fill, img), text=text)
    fill, _ = load(a.folder / f'{a.run}-A-fill.pdf')
    img, text = load(a.folder / f'{a.run}-C-effect.pdf')
    res['C']['effect'] = dict(damage(fill, img), text=text)
    print(json.dumps(res, indent=1))
    if a.out:
        a.out.write_text(json.dumps(res, indent=1) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
