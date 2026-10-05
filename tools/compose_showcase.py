"""Set the StrokeMaster showcase's exports side by side (tools/jsx/build-showcase.jsx).

Each case of showcase.json is cropped from showcase-before.pdf (StrokeMaster off: InDesign
alone) and from showcase-after.pdf (the default switches), or for the separate-stories case
from showcase-stories.pdf, and placed as vector content, before on the left, after on the
right, with a note under each pair. The cases StrokeMaster fixes come first, then the forms it
handles with the least certainty.

usage: compose_showcase.py <showcase folder> [--out PDF] [--engine HASH] [--panel HASH]
"""
import argparse
import json
import os

import pymupdf

NOTES = {
    'F1': 'Each letter of an Outside-stroked line is drawn stroke first, then fill, in turn, so with tight tracking a letter\'s stroke covers the fill of the letter before it. StrokeMaster draws every stroke of the line beneath every fill.',
    'F2': 'The same across the threaded frames of one story: the second frame\'s strokes no longer cover the first frame\'s letters.',
    'F3': 'Miter joins at the sharp joints of M, V, W, A, and z stick out of InDesign\'s text strokes as spikes; StrokeMaster removes the part that escapes the letter.',
    'F4': 'Past the miter limit (4), InDesign cuts a thin triangle\'s point flat at its tip. StrokeMaster keeps a shape\'s own points whole when both edges are straight and long enough.',
    'F5': 'InDesign strokes an Outside outline by offsetting it; on script outlines the offset breaks, flooding openings and stroking contour edges inside the letters. StrokeMaster fills the outline itself and strokes only around the filled shape.',
    'F6': 'InDesign\'s Inside stroke offsets the outline inward; where a part is thinner than the stroke, the offset drops it or spills past it. StrokeMaster fills the outline and clips the stroke to it.',
    'F7': 'Fancy corner effects cut each corner of a glyph into tiny zigzags, whose miters stick out as long points. StrokeMaster bevels them past the limit and trims them within it.',
    'F8': 'Overlapping contours of one shape: InDesign strokes every contour, edges inside the shape included. StrokeMaster strokes the outline of the filled shape.',
    'F9': 'Dashes: InDesign dashes each contour, so where letters join or shapes overlap, the dashes cross. StrokeMaster has InDesign\'s own dash stroker draw along one merged outline per word.',
    'F10': 'Stripes: the same, for the built-in striped styles, which StrokeMaster draws itself from their measured layouts.',
    'F11': 'Patterned strokes past the miter limit: InDesign bevels them; StrokeMaster adds the whole point, solid (it does not carry the pattern).',
    'F12': 'Tapered script tips meet a curve at a sharp angle; their miters, within the limit, stuck out as spikes (under W). StrokeMaster trims miters at corners sharper than 60 degrees beside a curve or a very short edge.',
    'L1': 'Least sure: StrokeMaster draws the built-in stripes itself, with layouts measured from InDesign\'s output, the first stripe on the side away from the object. Around counters (B, O, K\'s inner corners) the orientation is my choice; InDesign follows each contour\'s direction.',
    'L2': 'Least sure: stripes with a gap color are left to InDesign (a gap band beneath stripes showed through), so these still cross between letters.',
    'L3': 'Least sure: Wavy, the hash styles, diamonds, and custom stripe styles keep InDesign\'s pattern on each contour, clipped outside the fill; they still cross between letters.',
    'L4': 'Least sure: two separate, unthreaded stories that overlap. InDesign stacks them; a halo over another frame is often deliberate. The right side shows the experimental "Across separate stories" switch, which is off by default.',
    'L5': 'Least sure: text in a frame both sheared and rotated escapes about as much with StrokeMaster as without (measured 94 and 88 square points); the cause was not found.',
    'L6': 'Least sure: a decorative font with thousands of tiny corners. Drawn, but slow to prepare the first time and large in PDF (every join is a filled shape).',
    'L7': 'Least sure: an open path with a Center Miter stroke. Whole points are kept past the limit, next to InDesign\'s caps at the ends.',
    'L8': 'Least sure: Inside strokes with inset corner effects: the tiny holes the effect\'s loops leave are not stroked; the fill keeps them.',
    'L9': 'Least sure: object-level opacity and a drop shadow, which InDesign applies around StrokeMaster\'s stroke. Stroke-level or fill-level opacity and effects stay InDesign\'s.',
    'L10': 'Least sure: brush serifs. Their sharp tips count as glyph tips, so their miters within the limit are trimmed; the halo loses some of its points.',
}

MARGIN, GAP, NOTE_H, TOP = 24, 24, 46, 56


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('folder')
    ap.add_argument('--out')
    ap.add_argument('--engine', default='')
    ap.add_argument('--panel', default='')
    a = ap.parse_args()
    meta = json.load(open(os.path.join(a.folder, 'showcase.json'), encoding='utf-8'))
    src = {v: pymupdf.open(os.path.join(a.folder, 'showcase-%s.pdf' % v)) for v in ('before', 'after', 'stories')}
    cells = meta['cells']
    cw = max(c['bounds'][3] - c['bounds'][1] for c in cells)
    W = 2 * cw + GAP + 2 * MARGIN
    H = 1400
    out = pymupdf.open()

    page = out.new_page(width=W, height=H)
    lines = [
        ('StrokeMaster: before and after', 22, True),
        ('Left: InDesign alone (every switch off). Right: StrokeMaster with its default switches.', 12, False),
        ('Page 2 onward: what it fixes (F1 to F12), then the forms it handles with the least certainty (L1 onward).', 12, False),
        ('', 8, False),
        ('Testing it in InDesign: Window > StrokeMaster opens the panel. Each checkbox turns one correction on or off,', 12, False),
        ('and every open document redraws at once; Restore Defaults puts them all back. The switches apply to every', 12, False),
        ('document and to export, and are kept between sessions. The document behind this PDF, showcase.indd,', 12, False),
        ('is next to it: open it in InDesign and flip the switches to see each case change.', 12, False),
        ('', 8, False),
        ('Builds: engine %s, panel %s; InDesign %s.' % (a.engine or '?', a.panel or '?', meta.get('app', '?')), 11, False),
    ]
    y = 80
    for text, size, bold in lines:
        page.insert_text((MARGIN, y), text, fontsize=size, fontname='hebo' if bold else 'helv')
        y += size + 10

    def new_page(title):
        p = out.new_page(width=W, height=H)
        p.insert_text((MARGIN, 30), title, fontsize=14, fontname='hebo')
        p.insert_text((MARGIN, 48), 'InDesign alone', fontsize=11, fontname='helv')
        p.insert_text((MARGIN + cw + GAP, 48), 'StrokeMaster', fontsize=11, fontname='helv')
        return p

    page, y, kind = None, H, None
    for c in cells:
        x0, y0 = c['bounds'][1], c['bounds'][0]
        x1, y1 = c['bounds'][3], c['bounds'][2]
        h = y1 - y0
        if page is None or kind != c['kind'] or y + h + NOTE_H > H - MARGIN:
            kind = c['kind']
            page = new_page('What StrokeMaster fixes' if kind == 'fix' else 'Least certain')
            y = TOP
        clip = pymupdf.Rect(x0, y0, x1, y1)
        right = 'stories' if c['id'] == 'L4' else 'after'
        page.show_pdf_page(pymupdf.Rect(MARGIN, y, MARGIN + cw, y + h), src['before'], 0, clip=clip)
        page.show_pdf_page(pymupdf.Rect(MARGIN + cw + GAP, y, MARGIN + 2 * cw + GAP, y + h), src[right], 0, clip=clip)
        page.draw_rect(pymupdf.Rect(MARGIN, y, MARGIN + cw, y + h), color=(0.75, 0.75, 0.75), width=0.5)
        page.draw_rect(pymupdf.Rect(MARGIN + cw + GAP, y, MARGIN + 2 * cw + GAP, y + h), color=(0.75, 0.75, 0.75), width=0.5)
        note = NOTES.get(c['id'], '')
        if right == 'stories':
            note += ' (Right: "Across separate stories" on.)'
        page.insert_textbox(pymupdf.Rect(MARGIN, y + h + 4, W - MARGIN, y + h + NOTE_H), note, fontsize=10, fontname='helv')
        y += h + NOTE_H + 8

    out.set_metadata({'title': 'StrokeMaster showcase', 'author': '', 'creator': '', 'producer': '', 'subject': '', 'keywords': ''})
    target = a.out or os.path.join(a.folder, 'StrokeMaster showcase.pdf')
    out.save(target, garbage=4, deflate=True)
    print(target, out.page_count, 'pages')


if __name__ == '__main__':
    main()
