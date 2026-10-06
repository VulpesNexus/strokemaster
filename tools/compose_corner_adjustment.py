"""Set out the corner adjustment study's exports as a grid (tools/jsx/build-corner-adjustment.jsx).

For each stroke type and subject, one block: the four corner adjustments side by side, InDesign
alone (corner-adjustment-before.pdf, StrokeMaster off) above StrokeMaster at its default
switches (corner-adjustment-after.pdf), each cell placed as vector content.

usage: compose_corner_adjustment.py <folder> [--out PDF] [--engine HASH]
"""
import argparse
import json
import os

import pymupdf

MARGIN, LABEL_W, GAP, TITLE_H, HEAD_H = 24, 92, 8, 22, 16

COVER = [
    ('Corner adjustment: InDesign alone and StrokeMaster', 20, True),
    ('InDesign\'s Stroke panel offers four corner adjustments for dashed strokes (Corners: None, Adjust', 11, False),
    ('dashes, Adjust gaps, Adjust dashes and gaps); a custom dotted or dash style carries its own. With one on,', 11, False),
    ('InDesign fits the pattern between every two anchors of the path, so a dash or dot sits at each anchor.', 11, False),
    ('Each block shows one subject under the four settings: InDesign alone on the top row, StrokeMaster', 11, False),
    ('(default switches) below. Outside strokes, 3 pt, miter limit 4.', 11, False),
    ('', 6, False),
    ('What it shows:', 11, True),
    ('- Square and ellipse: every setting looks the same with or without StrokeMaster.', 11, False),
    ('- Star: InDesign alone draws its Outside dashes and dots inside the star. StrokeMaster draws them', 11, False),
    ('  outside, along one outline it rebuilds with the star\'s own ten corners, so each setting still works.', 11, False),
    ('- Overlapping squares: one outline instead of two crossing ones, under every setting.', 11, False),
    ('- Outlined text: a letter\'s outline has an anchor every few points. InDesign fits the pattern between', 11, False),
    ('  every two of them, so with any setting but None Minion\'s dashes crowd at the serifs and on script', 11, False),
    ('  they run together almost solid. StrokeMaster fits it between the outline\'s real corners only (turns', 11, False),
    ('  over 30 degrees): the setting\'s own pattern, a dash or dot centered on each real corner, the rest', 11, False),
    ('  laid evenly between, as InDesign lays it on a shape\'s corners.', 11, False),
    ('- A point past the miter limit: StrokeMaster adds the whole point, solid from the corner, over the', 11, False),
    ('  pattern, under every setting.', 11, False),
    ('- The None column keeps InDesign\'s None: the pattern runs on regardless of corners, so a spike shows', 11, False),
    ('  only where a dash happens to cover it, as in InDesign alone.', 11, False),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('folder')
    ap.add_argument('--out')
    ap.add_argument('--engine', default='')
    a = ap.parse_args()
    meta = json.load(open(os.path.join(a.folder, 'corner-adjustment.json'), encoding='utf-8'))
    src = {v: pymupdf.open(os.path.join(a.folder, 'corner-adjustment-%s.pdf' % v)) for v in ('before', 'after')}
    cells = meta['cells']
    cw = cells[0]['bounds'][3] - cells[0]['bounds'][1]
    ch = cells[0]['bounds'][2] - cells[0]['bounds'][0]
    W = MARGIN * 2 + LABEL_W + 4 * cw + 3 * GAP
    block = TITLE_H + HEAD_H + 2 * ch + GAP + 18
    H = MARGIN * 2 + 4 * block
    out = pymupdf.open()

    page = out.new_page(width=W, height=H)
    y = 70
    lines = COVER + [('', 6, False), ('Builds: engine %s; InDesign %s.' % (a.engine or '?', meta.get('app', '?')), 10, False)]
    for text, size, bold in lines:
        page.insert_text((MARGIN, y), text, fontsize=size, fontname='hebo' if bold else 'helv')
        y += size + 9

    blocks = {}
    for c in cells:
        blocks.setdefault((c['section'], c['row']), []).append(c)
    page, y = None, H
    for key in sorted(blocks):
        group = sorted(blocks[key], key=lambda c: c['col'])
        if page is None or y + block > H - MARGIN:
            page = out.new_page(width=W, height=H)
            y = MARGIN
        page.insert_text((MARGIN, y + 14), group[0]['title'], fontsize=12, fontname='hebo')
        y += TITLE_H
        for c in group:
            x = MARGIN + LABEL_W + c['col'] * (cw + GAP)
            page.insert_text((x, y + 11), c['adjust'], fontsize=10, fontname='helv')
        y += HEAD_H
        for row, (name, variant) in enumerate((('InDesign alone', 'before'), ('StrokeMaster', 'after'))):
            ry = y + row * (ch + GAP)
            page.insert_text((MARGIN, ry + ch / 2), name, fontsize=10, fontname='helv')
            for c in group:
                x = MARGIN + LABEL_W + c['col'] * (cw + GAP)
                t, l, b, r = c['bounds']
                rect = pymupdf.Rect(x, ry, x + cw, ry + ch)
                page.show_pdf_page(rect, src[variant], 0, clip=pymupdf.Rect(l, t, r, b))
                page.draw_rect(rect, color=(0.75, 0.75, 0.75), width=0.5)
        y += 2 * ch + GAP + 18

    out.set_metadata({'title': 'StrokeMaster corner adjustment', 'author': '', 'creator': '', 'producer': '', 'subject': '', 'keywords': ''})
    target = a.out or os.path.join(a.folder, 'StrokeMaster corner adjustment.pdf')
    out.save(target, garbage=4, deflate=True)
    print(target, out.page_count, 'pages')


if __name__ == '__main__':
    main()
