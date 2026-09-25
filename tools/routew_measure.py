"""Measurements for the route W certification: fill-core damage, edge variance, PDF structure.

Pure functions over grayscale renders (uint8 arrays) and PDF files; tools/certify_routew.py
drives them over a session's outputs. Nothing here launches Adobe.

Fill-core damage (the pass criterion): pixels inside the eroded fill mask more than 64 levels
from the fill-only twin, rendered by the same renderer at the same resolution. A correct stroke
never reaches a fill's interior, so the core must be identical up to color conversion.

Edge variance (against the split reference, victim census only): every differing pixel is
placed by its distance from the nearest reference edge. Differences within the anti-aliasing
band (two device pixels of an edge) are rasterization variance; differences over 64 levels
farther out are semantic. Along each white-stroke boundary the signed coverage change divided
by the boundary length gives the mean outward offset of the stroke edge, in device pixels.
"""
import re
from collections import Counter

import numpy
from PIL import Image
from scipy import ndimage

DAMAGE = 64
BAND_PX = 2
ERODE_AT_600 = 6
PAD_PT = 14.0


def gray(path):
    im = Image.open(path)
    if im.mode in ('RGBA', 'LA', 'PA') or (im.mode == 'P' and 'transparency' in im.info):
        im = im.convert('RGBA')
        white = Image.new('RGBA', im.size, (255, 255, 255, 255))
        im = Image.alpha_composite(white, im)
    return numpy.asarray(im.convert('L'))


def erode_px(ppi):
    return max(1, int(round(ERODE_AT_600 * ppi / 600.0)))


def core_of(mask, ppi):
    return ndimage.binary_erosion(mask >= 254, iterations=erode_px(ppi))


def frame_box(bounds, ppi, shape, pad_pt=PAD_PT):
    y1, x1, y2, x2 = bounds
    k = ppi / 72.0
    h, w = shape
    return (slice(max(0, int((y1 - pad_pt) * k)), min(h, int(round((y2 + pad_pt) * k)))),
            slice(max(0, int((x1 - pad_pt) * k)), min(w, int(round((x2 + pad_pt) * k)))))


def align(*arrays):
    h = min(a.shape[0] for a in arrays)
    w = min(a.shape[1] for a in arrays)
    return [a[:h, :w] for a in arrays], max(max(a.shape[0] for a in arrays) - h, max(a.shape[1] for a in arrays) - w)


def core_damage(out, twin, core, frames, ppi):
    """Per frame: core pixels, pixels more than DAMAGE levels from the twin, maximum difference."""
    (out, twin, core), trimmed = align(out, twin, core)
    diff = numpy.abs(out.astype(numpy.int16) - twin.astype(numpy.int16))
    rows = []
    for f in frames:
        if 'bounds' not in f:
            rows.append({'key': f['key'], 'error': f.get('buildError', 'no bounds')})
            continue
        sl = frame_box(f['bounds'], ppi, core.shape, 0.0)
        c, d = core[sl], diff[sl]
        rows.append({'key': f['key'], 'core_px': int(c.sum()), 'damage_px': int((c & (d > DAMAGE)).sum()),
                     'core_max_diff': int(d[c].max()) if c.any() else 0})
    return {'ppi': ppi, 'erode_px': erode_px(ppi), 'trimmed_px': trimmed, 'frames': rows,
            'damage_px_total': sum(r.get('damage_px', 0) for r in rows)}


def edges_of(ref):
    r = ref.astype(numpy.int16)
    gy = numpy.abs(numpy.diff(r, axis=0, append=r[-1:, :]))
    gx = numpy.abs(numpy.diff(r, axis=1, append=r[:, -1:]))
    return numpy.maximum(gx, gy) > 32


def white_boundary_offset(out, ref, backdrop):
    """Mean outward offset, in device pixels, of white-on-backdrop edges: the signed coverage
    change over the band around each edge divided by the edge length (4-neighbor transitions)."""
    span = 255.0 - backdrop
    if span < 32:
        return None
    cov_ref = numpy.clip((ref.astype(float) - backdrop) / span, 0, 1)
    cov_out = numpy.clip((out.astype(float) - backdrop) / span, 0, 1)
    solid = cov_ref > 0.5
    length = int((solid[1:, :] != solid[:-1, :]).sum() + (solid[:, 1:] != solid[:, :-1]).sum())
    if not length:
        return None
    near = ndimage.binary_dilation(solid, iterations=3) & ~ndimage.binary_erosion(solid, iterations=3)
    near &= (ref.astype(int) >= backdrop - 8)
    return {'offset_px': float((cov_out - cov_ref)[near].sum() / length), 'edge_length_px': length}


def edge_metrics(out, ref, frames, ppi, backdrop=None):
    """Per frame, against the split reference: how much differs, where, and how far from an edge."""
    (out, ref), trimmed = align(out, ref)
    rows = []
    for f in frames:
        sl = frame_box(f['bounds'], ppi, ref.shape)
        o, r = out[sl].astype(numpy.int16), ref[sl].astype(numpy.int16)
        d = numpy.abs(o - r)
        dist = ndimage.distance_transform_edt(~edges_of(r.astype(numpy.uint8)))
        diff = d > 0
        ys, xs = numpy.nonzero(d > 16)
        row = {'key': f['key'], 'px_diff': int(diff.sum()), 'px_over_16': int((d > 16).sum()), 'px_over_64': int((d > 64).sum()),
               'max_diff': int(d.max()), 'mean_abs_diff_frame': round(float(d.mean()), 4),
               'mean_abs_diff_where_diff': round(float(d[diff].mean()), 2) if diff.any() else 0.0,
               'bbox_over_16_px': [int(ys.min()), int(xs.min()), int(ys.max()), int(xs.max())] if len(ys) else None,
               'over_16_by_edge_distance_px': {str(k): int(((d > 16) & (numpy.minimum(numpy.floor(dist), 3) == k)).sum()) for k in range(4)},
               'over_64_beyond_band': int(((d > DAMAGE) & (dist > BAND_PX)).sum()),
               'net_signed_sum': int((o - r).sum())}
        if backdrop is not None:
            row['white_edge'] = white_boundary_offset(out[sl], ref[sl], backdrop)
        rows.append(row)
    return {'ppi': ppi, 'trimmed_px': trimmed, 'band_px': BAND_PX, 'frames': rows}


# ---- PDF structure

OPS = re.compile(rb'(?<![A-Za-z*\'"])(BT|ET|Tj|TJ|\'|"|Tr|S|s|f\*|f|F|B\*|B|b\*|b|W\*|W|n|Do|sh|BI|gs)(?![A-Za-z*])')


def _content_ops(pdf, page):
    import pikepdf
    counts, tr = Counter(), Counter()
    images, forms, tgroups, softmask_gs = 0, 0, 0, 0
    seen = set()

    def walk(obj_with_contents, resources, depth):
        nonlocal images, forms, tgroups, softmask_gs
        try:
            instructions = pikepdf.parse_content_stream(obj_with_contents)
        except Exception:
            return
        for operands, op in instructions:
            name = str(op)
            counts[name] += 1
            if name == 'Tr' and operands:
                tr[int(operands[0])] += 1
            if name == 'gs' and resources is not None and operands:
                g = resources.get('/ExtGState', {}).get(str(operands[0]))
                if g is not None and ('/SMask' in g and str(g.SMask) != '/None' or float(g.get('/ca', 1)) < 1 or float(g.get('/CA', 1)) < 1):
                    softmask_gs += 1
            if name == 'Do' and resources is not None and operands:
                x = resources.get('/XObject', {}).get(str(operands[0]))
                if x is None:
                    continue
                key = x.objgen
                if x.get('/Subtype') == '/Image':
                    images += 1
                elif x.get('/Subtype') == '/Form':
                    forms += 1
                    if '/Group' in x and x.Group.get('/S') == '/Transparency':
                        tgroups += 1
                    if key not in seen and depth < 8:
                        seen.add(key)
                        walk(x, x.get('/Resources', resources), depth + 1)
            if name == 'BI':
                images += 1

    walk(page, page.obj.get('/Resources'), 0)
    return counts, tr, images, forms, tgroups, softmask_gs


def pdf_fonts(pdf):
    import pikepdf
    fonts = {}
    for obj in pdf.objects:
        if isinstance(obj, pikepdf.Dictionary) and obj.get('/Type') == '/Font' and '/BaseFont' in obj:
            base = str(obj.BaseFont)[1:]
            desc = obj.get('/FontDescriptor')
            if desc is None and '/DescendantFonts' in obj:
                desc = obj.DescendantFonts[0].get('/FontDescriptor')
            embedded = bool(desc is not None and any(k in desc for k in ('/FontFile', '/FontFile2', '/FontFile3')))
            fonts[base] = {'subtype': str(obj.get('/Subtype')), 'embedded': embedded,
                           'subset': bool(re.match(r'^[A-Z]{6}\+', base))}
    return fonts


def frame_of(rect, frames, page_rect, tol=15):
    if (rect[2] - rect[0]) * (rect[3] - rect[1]) > 0.9 * page_rect.width * page_rect.height:
        return None
    cx, cy = (rect[0] + rect[2]) / 2, (rect[1] + rect[3]) / 2
    for f in frames:
        if 'bounds' not in f:
            continue
        y1, x1, y2, x2 = f['bounds']
        if x1 - tol <= cx <= x2 + tol and y1 - tol <= cy <= y2 + tol:
            return f['key']
    return '?'


def paint_order(page, frames):
    """Per frame, in painting order: path strokes and fills, and text by render mode.
    stroke_after_fill_text lists strokes painted after the frame's first fill-mode glyph."""
    events = []
    for d in page.get_drawings():
        kind = {'s': 'path-stroke', 'f': 'path-fill', 'fs': 'path-fill+stroke'}.get(d['type'], d['type'])
        events.append((d['seqno'], kind, d.get('width'), frame_of(d['rect'], frames, page.rect), 0))
    for s in page.get_texttrace():
        kind = {0: 'text-fill', 1: 'text-stroke', 2: 'text-fill+stroke', 3: 'text-invisible', 7: 'text-clip'}.get(s['type'], 'text-%d' % s['type'])
        events.append((s['seqno'], kind, s.get('linewidth'), frame_of(s['bbox'], frames, page.rect), len(s['chars'])))
    events.sort(key=lambda e: e[0])
    per = {}
    for _, kind, width, key, nchars in events:
        if key is None:
            continue
        per.setdefault(key, []).append((kind, width, nchars))
    rows = []
    for f in frames:
        seq = per.get(f['key'], [])
        first_fill = next((i for i, (k, _, _) in enumerate(seq) if k in ('text-fill', 'text-fill+stroke')), None)
        after = [] if first_fill is None else [round(w or 0, 3) for k, w, _ in seq[first_fill:] if k in ('path-stroke', 'text-stroke', 'path-fill+stroke', 'text-fill+stroke')]
        rows.append({'key': f['key'],
                     'glyphs': {k: sum(n for kk, _, n in seq if kk == k) for k in ('text-fill', 'text-stroke', 'text-fill+stroke', 'text-invisible', 'text-clip') if any(kk == k for kk, _, _ in seq)},
                     'path_strokes': sum(1 for k, _, _ in seq if k == 'path-stroke'), 'path_fills': sum(1 for k, _, _ in seq if k == 'path-fill'),
                     'strokes_before_first_fill_text': sum(1 for k, _, _ in seq[:first_fill if first_fill is not None else len(seq)] if k in ('path-stroke', 'text-stroke')),
                     'stroke_after_fill_text': after})
    unknown = sum(1 for e in events if e[3] == '?')
    return rows, unknown


def pdf_structure(path, frames):
    import pikepdf
    import pymupdf
    head = open(path, 'rb').read(16)
    version = head[5:8].decode('ascii', 'replace')
    with pikepdf.open(path) as pdf:
        page = pdf.pages[0]
        counts, tr, images, forms, tgroups, soft = _content_ops(pdf, page)
        fonts = pdf_fonts(pdf)
        pdfx = str(pdf.docinfo.get('/GTS_PDFXVersion', '')) or None
        intents = len(pdf.Root.get('/OutputIntents', []))
        page_group = bool('/Group' in page.obj and page.obj.Group.get('/S') == '/Transparency')
        editing = any(k for k in ('/PieceInfo',) if k in page.obj or k in pdf.Root)
    doc = pymupdf.open(path)
    pg = doc[0]
    order, unknown = paint_order(pg, frames)
    text = pg.get_text()
    return {'pdf_version': version, 'pdfx': pdfx, 'output_intents': intents, 'page_transparency_group': page_group,
            'piece_info': editing, 'fonts': fonts, 'text_chars': len(re.sub(r'\s', '', text)),
            'ops': {k: counts[k] for k in ('BT', 'Tj', 'TJ', 'S', 's', 'f', 'f*', 'B', 'W', 'W*', 'sh', 'Do', 'gs') if counts[k]},
            'text_render_modes': {str(k): v for k, v in sorted(tr.items())},
            'images': images, 'form_xobjects': forms, 'transparency_groups': tgroups, 'soft_or_alpha_gs': soft,
            'frames': order, 'unknown_frame_events': unknown}
