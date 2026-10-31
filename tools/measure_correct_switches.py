"""Measure the switches test (tools/jsx/ai-correct-switches.jsx) without Adobe.

The story route's color-aware metric (measure_story_route.py): 600 ppi renders; a fill-core pixel
whose color differs from the stroke-free export is damaged. Each stage is also compared with the
uncorrected export ("native"): a stage that takes every correction off should draw as it does.
Text is counted two ways: MuPDF's extraction, which merges a glyph drawn twice in one place as
Acrobat does, and every glyph drawn as text (fill and stroke passes counted apart), which is what
a viewer that does not merge them extracts.
"""
import argparse
import json
import re
from pathlib import Path

import pymupdf

from measure_story_route import DPI, PT2, core_of, differs, render

# Each case's band of the page, in points from the top (the fixture's layout).
BANDS = {"story": (0, 210), "separate": (210, 330), "miter": (330, 450), "dashes": (450, 760)}


def banded(mask):
    out = {}
    for name, (top, bottom) in BANDS.items():
        a, b = round(top * DPI / 72), round(bottom * DPI / 72)
        out[name] = round(int(mask[a:b].sum()) * PT2, 2)
    return out


def drawn_glyphs(path):
    with pymupdf.open(path) as doc:
        return sum(len(span["chars"]) for span in doc[0].get_texttrace())


def measure(folder, run):
    folder = Path(folder)
    receipt = json.loads((folder / f"{run}-results.json").read_text(encoding="utf-8-sig"))
    fill, fill_text = render(folder / f"{run}-fill.pdf")
    native, _ = render(folder / f"{run}-native.pdf")
    core = core_of(fill)
    stages = {"native": {"damagePt2": banded(core & differs(native, fill)),
                         "glyphsDrawn": drawn_glyphs(folder / f"{run}-native.pdf")}}
    for name, row in receipt["stages"].items():
        path = folder / f"{run}-{name}.pdf"
        image, text = render(path)
        stages[name] = {"damagePt2": banded(core & differs(image, fill)),
                        "differsFromNativePt2": banded(differs(image, native)),
                        "characters": len(text), "glyphsDrawn": drawn_glyphs(path),
                        "groups": row.get("groups"), "correct": json.loads(row["correct"])}
    record = {"schema": 1, "run": run, "sourceCharacters": len(fill_text),
              "fillGlyphsDrawn": drawn_glyphs(folder / f"{run}-fill.pdf"), "stages": stages}
    for key in ("uiStatus0", "uiStatusAfterTab", "tab", "limitsBefore", "limitsOn", "limitsOff",
                "squareBefore", "squareOn", "squareOff"):
        if key in receipt:
            record[key] = receipt[key]
    # The route stroke's limit with No miter spikes on, then off (the style dumps).
    limits = [re.findall(r"STROKE w 7 join 0 limit ([0-9.]+)", receipt.get(k, "")) for k in ("dumpOn", "dumpOff")]
    record["routeLimit"] = [float(x[-1]) if x else None for x in limits]
    return record


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folder", type=Path)
    parser.add_argument("run")
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    record = measure(args.folder, args.run)
    text = json.dumps(record, indent=2) + "\n"
    if args.out:
        args.out.write_text(text, encoding="utf-8")
    print(text)


if __name__ == "__main__":
    main()
