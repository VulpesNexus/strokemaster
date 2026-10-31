"""Measure the synthetic Illustrator text parity sweep without Adobe.

Uses the existing 600 ppi / three-pixel erosion / gray <32 fill-core metric.
Requires identical page dimensions; reports export text and reopen pixel drift.
"""
import argparse
import json
from pathlib import Path

import numpy as np
import pymupdf
from scipy import ndimage


def render(path):
    with pymupdf.open(path) as doc:
        if len(doc) != 1:
            raise ValueError("expected a single page")
        page = doc[0]
        pix = page.get_pixmap(dpi=600, colorspace=pymupdf.csGRAY, alpha=False)
        glyphs = []
        for block in page.get_text("rawdict")["blocks"]:
            for line in block.get("lines", []):
                for span in line["spans"]:
                    for char in span["chars"]:
                        if not char["c"].isspace():
                            glyphs.append((char["c"], *[round(v, 2) for v in char["origin"]]))
        return (np.frombuffer(pix.samples, np.uint8).reshape(pix.height, pix.width).copy(),
                "".join(page.get_text().split()), set(glyphs))


def measure(folder, run):
    host = json.loads((folder / f"{run}-results.json").read_text(encoding="utf-8-sig"))
    result = {"run": run, "font": host["font"], "host": host["host"], "cases": []}
    for row in host["rows"]:
        if "error" in row:
            result["cases"].append({"error": row["error"], "stage": row["stage"], "passed": False})
            continue
        kind = row["kind"]
        fill, fill_text, fill_glyphs = render(folder / f"{run}-{kind}-fill.pdf")
        core = ndimage.binary_erosion(fill < 32, iterations=3)
        item = {"kind": kind, "corePixels": int(core.sum()),
                "stylesPreserved": row["stylesPreserved"],
                "reopenPreserved": row["reopenPreserved"], "editable": row.get("editable", False),
                "ms": row.get("ms"), "correctionMs": row.get("correctionMs"),
                "fillExportMs": row.get("fillExportMs"), "nativeExportMs": row.get("nativeExportMs"),
                "exports": {}}
        route_image = None
        for variant in ("native", "route", "reopen"):
            image, text, glyphs = render(folder / f"{run}-{kind}-{variant}.pdf")
            if image.shape != fill.shape:
                raise ValueError(f"page dimension mismatch: {run}/{kind}/{variant}")
            damaged = int(np.count_nonzero(core & (image >= 32)))
            export = {"damagedCorePixels": damaged, "damagedCorePt2": damaged * (72 / 600) ** 2,
                      "textMatchesFill": text == fill_text, "visibleTextCharacters": len(text),
                      "uniqueGlyphs": len(glyphs), "uniqueGlyphsMatchFill": glyphs == fill_glyphs}
            item["exports"][variant] = export
            if variant == "route":
                route_image = image
            if variant == "reopen":
                item["reopenChangedPixels"] = int(np.count_nonzero(image != route_image))
                delta = np.abs(image.astype(np.int16) - route_image.astype(np.int16))
                item["reopenMaxGrayDelta"] = int(delta.max())
                item["reopenMeanGrayDelta"] = float(delta.mean())
                item["reopenChangedAreaPt2"] = item["reopenChangedPixels"] * (72 / 600) ** 2
        item["passed"] = bool(item["corePixels"] and item["stylesPreserved"]
                              and item["reopenPreserved"] and item["editable"]
                              and all(v["textMatchesFill"] and v["visibleTextCharacters"] > 0
                                      for v in item["exports"].values())
                              and item["exports"]["route"]["damagedCorePixels"] == 0
                              and item["exports"]["reopen"]["damagedCorePixels"] == 0
                              and item["reopenChangedPixels"] == 0)
        result["cases"].append(item)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folder", type=Path)
    parser.add_argument("runs", nargs="+")
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    results = [measure(args.folder, run) for run in args.runs]
    cases = [case for run in results for case in run["cases"]]
    record = {"schema": 1, "dpi": 600, "erodePixels": 3, "darkLevel": 32,
              "total": len(cases), "passed": sum(case["passed"] for case in cases), "runs": results}
    measured = [case for case in cases if "exports" in case]
    record["gates"] = {
        "measuredCases": len(measured),
        "routeFillCoreUndamaged": sum(c["exports"]["route"]["damagedCorePixels"] == 0 for c in measured),
        "reopenFillCoreUndamaged": sum(c["exports"]["reopen"]["damagedCorePixels"] == 0 for c in measured),
        "formattingPreserved": sum(c["stylesPreserved"] for c in measured),
        "liveTextEditable": sum(c["editable"] for c in measured),
        "reopenSnapshotExact": sum(c["reopenPreserved"] for c in measured),
        "reopenPixelsExact": sum(c["reopenChangedPixels"] == 0 for c in measured),
        "routeExtractionExact": sum(c["exports"]["route"]["textMatchesFill"] for c in measured),
    }
    args.out.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(record, indent=2))
    return 0 if record["passed"] == record["total"] and cases else 1


if __name__ == "__main__":
    raise SystemExit(main())
