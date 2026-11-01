"""Measure the story route color sweep (tools/jsx/ai-story-route.jsx) without Adobe.

Color-aware form of the text parity metric: 600 ppi RGB renders; the fill core is every
pixel of the stroke-free export with a channel below 160, eroded by three pixels. A core
pixel whose color differs by more than 48 on any channel from the stroke-free export is
damaged. Outside the fills (dilated by three pixels), the routed export should match the
native one: the strokes' own halo is not to change. The recolor check compares the routed
document after a user's recolor with the stroke-free export of the same recolor.
"""
import argparse
import json
from pathlib import Path

import numpy as np
import pymupdf
from scipy import ndimage

DPI, ERODE, INK, TOLERANCE = 600, 3, 160, 48
PT2 = (72 / DPI) ** 2


def render(path):
    with pymupdf.open(path) as doc:
        if len(doc) != 1:
            raise ValueError("expected a single page")
        page = doc[0]
        pix = page.get_pixmap(dpi=DPI, colorspace=pymupdf.csRGB, alpha=False)
        image = np.frombuffer(pix.samples, np.uint8).reshape(pix.height, pix.width, 3).astype(np.int16)
        return image, "".join(page.get_text().split())


def differs(a, b):
    return np.abs(a - b).max(axis=2) > TOLERANCE


def core_of(image):
    return ndimage.binary_erosion(image.min(axis=2) < INK, iterations=ERODE)


def measure_case(folder, run, kind):
    fill, fill_text = render(folder / f"{run}-{kind}-fill.pdf")
    native, _ = render(folder / f"{run}-{kind}-native.pdf")
    route, route_text = render(folder / f"{run}-{kind}-route.pdf")
    fill_recolor, fill_recolor_text = render(folder / f"{run}-{kind}-fill-recolor.pdf")
    recolor, recolor_text = render(folder / f"{run}-{kind}-recolor.pdf")
    if len({x.shape for x in (fill, native, route, fill_recolor, recolor)}) != 1:
        raise ValueError("page dimension mismatch")
    core = core_of(fill)
    outside = ~ndimage.binary_dilation(fill.min(axis=2) < INK, iterations=ERODE)
    recolor_core = core_of(fill_recolor)
    out = {"kind": kind, "corePixels": int(core.sum()),
           "nativeDamagePt2": round(int((core & differs(native, fill)).sum()) * PT2, 2),
           "routeDamagePt2": round(int((core & differs(route, fill)).sum()) * PT2, 2),
           "haloChangedPt2": round(int((outside & differs(route, native)).sum()) * PT2, 2),
           "recolorDamagePt2": round(int((recolor_core & differs(recolor, fill_recolor)).sum()) * PT2, 2),
           "sourceCharacters": len(fill_text), "routeCharacters": len(route_text),
           "recolorCharacters": len(recolor_text)}
    out["textExact"] = route_text == fill_text
    # The recolored route's text too (the third review of 2026-11-01).
    out["recolorTextExact"] = recolor_text == fill_recolor_text
    fixed_path = folder / f"{run}-{kind}-fixed.pdf"
    if fixed_path.exists():
        fixed, _ = render(fixed_path)
        out["fixedDamagePt2"] = round(int((core & differs(fixed, fill)).sum()) * PT2, 2)
        out["fixedHaloChangedPt2"] = round(int((outside & differs(fixed, native)).sum()) * PT2, 2)
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folder", type=Path)
    parser.add_argument("run")
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    receipt = json.loads((args.folder / f"{args.run}-results.json").read_text(encoding="utf-8-sig"))
    cases = []
    for row in receipt["rows"]:
        if "error" in row:
            cases.append({"error": row["error"], "stage": row["stage"]})
            continue
        case = measure_case(args.folder, args.run, row["kind"])
        route = row.get("route", "")
        gate = route.split('"appearance":{"gate":"', 1)[1].split('"', 1)[0] if '"appearance":{"gate":"' in route else "?"
        case.update({"gate": gate, "frames": row.get("frames"), "groups": row.get("groups"),
                     "stylesPreserved": json.dumps(row.get("before"), sort_keys=True) == json.dumps(row.get("after"), sort_keys=True)})
        cases.append(case)
    record = {"schema": 1, "host": receipt.get("host"), "font": receipt.get("font"), "build": receipt.get("build"),
              "run": args.run, "dpi": DPI, "erodePixels": ERODE, "inkBelow": INK, "tolerance": TOLERANCE, "cases": cases}
    text = json.dumps(record, indent=2) + "\n"
    if args.out:
        args.out.write_text(text, encoding="utf-8")
    print(text)


if __name__ == "__main__":
    main()
