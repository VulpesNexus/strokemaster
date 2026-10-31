"""Measure native story reopen and edit with StrokeMaster absent."""
import argparse
import json
from pathlib import Path

import numpy as np
from scipy import ndimage
from measure_text_parity import render


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folder", type=Path)
    parser.add_argument("run")
    parser.add_argument("reference_run")
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    receipt = json.loads((args.folder / f"{args.run}-results.json").read_text(encoding="utf-8-sig"))
    row = receipt["rows"][0]
    reference, reference_text, _ = render(args.folder / f"{args.reference_run}-threaded-route.pdf")
    reopened, reopen_text, _ = render(args.folder / f"{args.run}-threaded-reopen.pdf")
    edited, edit_text, _ = render(args.folder / f"{args.run}-threaded-edited.pdf")
    fill, fill_text, _ = render(args.folder / f"{args.run}-threaded-edited-fill.pdf")
    if len({x.shape for x in (reference, reopened, edited, fill)}) != 1:
        raise ValueError("page dimension mismatch")
    core = ndimage.binary_erosion(fill < 32, iterations=3)
    damaged = int(np.count_nonzero(core & (edited >= 32)))
    expected = "".join(row["expectedEdit"].split())
    record = {"schema": 1, "host": receipt["host"], "run": args.run,
              "dpi": 600, "erodePixels": 3, "darkLevel": 32,
              "pluginAbsence": receipt["pluginAbsence"],
              "reopenPreserved": row["reopenPreserved"], "editable": row["editable"],
              "reopenChangedPixels": int(np.count_nonzero(reopened != reference)),
              "reopenTextExact": reopen_text == reference_text,
              "editedCorePixels": int(core.sum()), "damagedCorePixels": damaged,
              "damagedCorePt2": damaged * (72 / 600) ** 2,
              "expectedCharacters": len(expected), "extractedCharacters": len(edit_text),
              "editedTextExact": edit_text == fill_text == expected}
    record["passed"] = bool(record["pluginAbsence"]["confirmed"] and record["reopenPreserved"]
                            and record["editable"] and record["reopenChangedPixels"] == 0
                            and record["reopenTextExact"] and record["editedCorePixels"]
                            and damaged == 0 and record["editedTextExact"])
    args.out.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(record, indent=2))
    return 0 if record["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
