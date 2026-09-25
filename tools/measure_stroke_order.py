"""Measure stroke-over-fill damage in PDFs of one fixture, without Adobe.

CLI: fill.pdf native.pdf corrected.pdf [more.pdf ...] --output result.json [--dpi 600]

fill.pdf is the fixture with every stroke removed. Its dark pixels are the
glyph-fill mask; the mask eroded by --erode pixels is the fill core, where no
antialiased edge can explain a lighter pixel. For every other PDF this reports:

- core_damaged_px: fill-core pixels lighter than --dark (a stroke painted over
  a fill that should be on top);
- ink_xor_px: pixels inked (darker than --ink) in exactly one of this PDF and
  the native PDF, a check that no stroke or fill coverage was lost or added;
- outside_changed_px: pixels outside the fill mask dilated by --erode that
  differ from native by more than --tolerance levels, i.e. visible stroke
  changes away from the fills;
- the PDF's paint sequence (paths collapsed), text characters, and fonts.

MuPDF renders every page; one renderer is not a claim about every renderer.
"""
import argparse
import hashlib
import json
from pathlib import Path

import numpy
import pikepdf
import pymupdf
from scipy import ndimage

PAINT = {'BT', 'ET', 'Tj', 'TJ', 'f', 'f*', 'F', 'S', 's', 'B', 'B*', 'b', 'b*'}


def render(path, dpi):
    with pymupdf.open(path) as doc:
        require(doc.page_count == 1, f'{path.name}: expected one page')
        pix = doc[0].get_pixmap(dpi=dpi, colorspace=pymupdf.csGRAY, alpha=False)
        return numpy.frombuffer(pix.samples, dtype=numpy.uint8).reshape(pix.height, pix.width).copy()


def require(condition, message):
    if not condition:
        raise SystemExit(message)


def paint_sequence(path):
    sequence = []
    pdf = pikepdf.open(path)  # keep the document alive while its page is parsed
    for operands, operator in pikepdf.parse_content_stream(pdf.pages[0]):
        op = str(operator)
        if op == 'Tr':
            sequence.append('Tr' + str(operands[0]))
        elif op in PAINT:
            sequence.append(op)
    return sequence


def strokes_before_fills(sequence):
    strokes = [i for i, op in enumerate(sequence) if op in ('S', 's')]
    fills = [i for i, op in enumerate(sequence) if op in ('f', 'f*', 'F')]
    return bool(strokes) and bool(fills) and max(strokes) < min(fills)


def measure(fill, native, image, erode, dark, ink, tolerance):
    mask = fill < 128
    core = ndimage.binary_erosion(mask, iterations=erode)
    outside = ~ndimage.binary_dilation(mask, iterations=erode)
    return {
        'core_px': int(core.sum()),
        'core_damaged_px': int((core & (image > dark)).sum()),
        'core_mean_level': float(image[core].mean()) if core.any() else None,
        'ink_xor_px': int(((image < ink) ^ (native < ink)).sum()),
        'outside_changed_px': int((outside & (numpy.abs(image.astype(int) - native.astype(int)) > tolerance)).sum()),
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('fill', type=Path)
    parser.add_argument('native', type=Path)
    parser.add_argument('others', type=Path, nargs='+')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--dpi', type=int, default=600)
    parser.add_argument('--erode', type=int, default=3)
    parser.add_argument('--dark', type=int, default=32)
    parser.add_argument('--ink', type=int, default=250)
    parser.add_argument('--tolerance', type=int, default=8)
    args = parser.parse_args(argv)
    require(not args.output.exists(), 'refusing to overwrite the output')
    fill = render(args.fill, args.dpi)
    native = render(args.native, args.dpi)
    require(fill.shape == native.shape, 'fill and native pages differ in size')
    report = {'schema': 1, 'renderer': f'MuPDF {pymupdf.VersionBind}', 'dpi': args.dpi,
              'thresholds': {'erode_px': args.erode, 'dark_level': args.dark, 'ink_level': args.ink,
                             'tolerance_levels': args.tolerance},
              'files': {}}
    for path in [args.native, *args.others]:
        image = native if path == args.native else render(path, args.dpi)
        require(image.shape == native.shape, f'{path.name}: page size differs')
        sequence = paint_sequence(path)
        with pymupdf.open(path) as doc:
            text_chars = len(doc[0].get_text().strip())
            fonts = len(doc[0].get_fonts())
        report['files'][path.name] = {
            'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
            **measure(fill, native, image, args.erode, args.dark, args.ink, args.tolerance),
            'paint_sequence': sequence, 'strokes_before_fills': strokes_before_fills(sequence),
            'text_chars': text_chars, 'fonts': fonts,
        }
    report['fill_sha256'] = hashlib.sha256(args.fill.read_bytes()).hexdigest()
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps({name: {k: v for k, v in r.items() if k not in ('paint_sequence', 'sha256')}
                      for name, r in report['files'].items()}, indent=1))


if __name__ == '__main__':
    main()
