"""Derives the StrokeMaster panel's icons from the owner's artwork, plugin/ui/icons/SM.svg.

The artwork is a crown on a wide artboard, inside a hidden square (the first <rect>); the panel
icon is that square, in two variants InDesign switches between with its interface brightness:

    StrokeMaster_N.*   dark art, for the light interfaces (resource types SVGA and PNGA)
    StrokeMaster_D.*   light art, for the dark interfaces (SVGD and PNGD)

as SVG and as PNGs at InDesign's palette icon sizes, 23 px and 46 px (the 2x copy,
IconPNGDefs.h's kHIDPIIconOffset). The crown has a 3 px transparent inset at 1x,
matching the measured 17 x 17 ink bounds of InDesign's native Layers icon on its
23 x 23 canvas. The grays are those of InDesign's own panel icons: #C2C2C2
measured on its medium-dark interface, and #464646, Spectrum's icon gray for light ones. The
same two-variant recipe as FreeDistort+'s tool icons (FDP_Tool_Lg_N and _D). Run from anywhere:

    python tools/make_panel_icons.py

Needs PyMuPDF to render the SVG and Pillow to write the PNGs.
"""
import re
from pathlib import Path

import pymupdf

ICONS = Path(__file__).resolve().parent.parent / 'plugin' / 'ui' / 'icons'
# The square native Layers panel icon has 17 px of ink on its 23 px canvas.
# Preserve the artwork; expand its viewBox to give the same 3 px inset at 1x.
CANVAS_PX = 23
INK_PX = 17

VARIANTS = {'N': '#464646', 'D': '#C2C2C2'}


def main():
    src = (ICONS / 'SM.svg').read_text(encoding='iso-8859-1')
    box = re.search(r'<rect x="([\d.]+)" y="([\d.]+)"[^>]*width="([\d.]+)" height="([\d.]+)"', src)
    x, y, w, h = (float(v) for v in box.groups())
    padding_x = w * (CANVAS_PX / INK_PX - 1) / 2
    padding_y = h * (CANVAS_PX / INK_PX - 1) / 2
    view_box = (x - padding_x, y - padding_y, w + 2 * padding_x, h + 2 * padding_y)
    art = src[src.index('<g>'):src.rindex('</g>') + 4]
    for name, color in VARIANTS.items():
        body = re.sub(r'style="fill:#[0-9A-Fa-f]{6};"', 'style="fill:%s;"' % color, art)
        svg = ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="%g %g %g %g" width="%g" height="%g">\n%s\n</svg>\n'
               % (*view_box, CANVAS_PX, CANVAS_PX, body))
        (ICONS / ('StrokeMaster_%s.svg' % name)).write_text(svg, encoding='utf-8', newline='\n')
        page = pymupdf.open(stream=svg.encode('utf-8'), filetype='svg')[0]
        for px in (23, 46):
            scale = px / page.rect.width
            page.get_pixmap(matrix=pymupdf.Matrix(scale, scale), alpha=True).save(ICONS / ('StrokeMaster_%s_%d.png' % (name, px)))
    print('wrote', ', '.join(sorted(p.name for p in ICONS.glob('StrokeMaster_*'))))


if __name__ == '__main__':
    main()
