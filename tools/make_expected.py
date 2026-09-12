"""Write the expected output for an overlap or fragmentation reproducer.

Usage:
    python tools/make_expected.py <issue-dir> <stem>

Reads <issue-dir>/actual/<stem>.pdf and its manifest. The expected output is that PDF with,
frame by frame, every Outside-stroke text object moved before the same frame's fill objects.
Nothing else changes: glyph positions, colors, and the stacking of separate frames stay as
InDesign exported them. Writes <issue-dir>/expected/<stem>-expected.pdf and a 300 dpi PNG.
"""
import importlib.util
import json
import os
import sys

import pikepdf
import pymupdf

HERE = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location("stroke_probe", os.path.join(HERE, "stroke_probe.py"))
sp = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(sp)


def main(issue_dir, stem):
    actual = os.path.join(issue_dir, "actual")
    pdf_path = os.path.join(actual, stem + ".pdf")
    frames = json.load(open(os.path.join(actual, stem + ".manifest.json"), encoding="utf-8"))["frames"]
    pdf = pikepdf.open(pdf_path)
    page = pdf.pages[0]
    height = float(page.MediaBox[3])
    blocks = sp.analyze(list(pikepdf.parse_content_stream(page)), page.get("/Resources"), sp.IDENT, None, height)["blocks"]
    sp.assign(blocks, frames)

    drop, insert, moved = set(), {}, 0
    for fi in range(len(frames)):
        by_form = {}
        for b in sorted((b for b in blocks if b["frame"] == fi), key=lambda b: b["start"]):
            by_form.setdefault(b["form"], []).append(b)
        for form, bs in by_form.items():
            strokes = [b for b in bs if b["role"] == "S"]
            if not strokes:
                continue
            for b in bs:
                drop.update((form, i) for i in b["shows"])
            ops = []
            for b in strokes + [b for b in bs if b["role"] != "S"]:
                ops.extend(sp.emit(b))
            insert[(form, bs[0]["start"])] = ops
            moved += len(strokes)

    out_dir = os.path.join(issue_dir, "expected")
    os.makedirs(out_dir, exist_ok=True)
    dst = os.path.join(out_dir, stem + "-expected.pdf")
    sp.edit_pdf(pdf_path, dst, drop, {}, insert, set())
    png = os.path.join(out_dir, stem + "-expected-300ppi.png")
    pymupdf.open(dst)[0].get_pixmap(dpi=300, alpha=False).save(png)
    print("moved %d stroke objects; wrote %s and %s" % (moved, os.path.basename(dst), os.path.basename(png)))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
