"""Re-certify the Illustrator native route in fresh hosts, as InDesign's C-runs do.

    python tools/certify_illustrator.py plan
    python tools/certify_illustrator.py run --out build/ai-cert-<n> --build region-prep-24 [--only PATTERN] [--budget SECONDS]
    python tools/certify_illustrator.py measure --out build/ai-cert-<n>
    python tools/certify_illustrator.py compare --out build/ai-cert-<n> --baseline tests/audit/results/<baseline>.json [--write-baseline]

`run` launches one fresh Illustrator per job through the isolated hidden desktop
(.workspace/tools/hidden-desktop-run.py), one case per process: several scripted documents in
one process have crashed Illustrator 30.8.2. It skips jobs whose receipt already exists, so a
run can be split across calls (--budget stops starting new jobs after that many seconds).
`measure` writes <out>/certification.json; `compare` judges it against a baseline and exits 1
on any failed line. Tolerances: damage may rise by at most max(1 pt2, 5 %) (2 pt2 and 10 % for
the halo, which edge anti-aliasing moves), extracted text must match exactly, gates must match.
Text is counted twice: as MuPDF extracts it ("text"), which merges a glyph drawn twice in one
place as Acrobat does, and as every glyph drawn as text ("glyphs"), fill and stroke passes apart,
which is what PDF-XChange extracts (the owner's hand test 11, 2026-10-30, saw letters doubled
there that the merged count could not show).
"""
import argparse
import fnmatch
import json
import os
import subprocess
import sys
import time
from pathlib import Path

import numpy as np
from scipy import ndimage

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from measure_story_route import INK, ERODE, PT2, core_of, differs, measure_case, render  # noqa: E402

HIDDEN = ROOT.parent / ".workspace" / "tools" / "hidden-desktop-run.py"


def glyphs(path):
    """Every glyph a PDF's first page draws as text, each fill or stroke pass counted."""
    import pymupdf
    with pymupdf.open(path) as doc:
        return sum(len(span["chars"]) for span in doc[0].get_texttrace())
FONTS = ["MinionPro-Regular", "SegoeScript", "KerberosFang", "GermanBeauty", "GreatVibes-Regular"]
MINION_CASES = ["rgb", "cmyk", "spot", "mixfill", "uniform", "mixboth", "perframe", "separate", "separate-off"]
OTHER_CASES = ["rgb", "mixfill", "uniform"]


def jobs():
    out = []
    for font in FONTS:
        for case in MINION_CASES if font == FONTS[0] else OTHER_CASES:
            out.append({"id": f"story-{font[:6].lower()}-{case}", "kind": "story", "font": font, "case": case,
                        "runner": "run-ai-story-route.ps1", "args": ["-Font", font, "-Cases", case, "-Fix"]})
    for case in ["hidden", "split3", "split4"]:
        out.append({"id": f"runs-{case}", "kind": "runs", "font": FONTS[0], "case": case,
                    "runner": "run-ai-story-runs.ps1", "args": ["-Font", FONTS[0], "-Cases", case]})
    # Correct Selection (the panel's button, bridge "correct") on the split story, and the panel itself.
    out.append({"id": "correct-split4", "kind": "runs", "font": FONTS[0], "case": "split4",
                "runner": "run-ai-story-runs.ps1", "args": ["-Font", FONTS[0], "-Cases", "split4", "-ViaCorrect"]})
    out.append({"id": "panel", "kind": "panel", "runner": "run-ai-panel.ps1", "args": []})
    # Switches that are off take their corrections off; one-frame stories joined; tooltips and Tab.
    out.append({"id": "switches", "kind": "switches", "runner": "run-ai-correct-switches.ps1", "args": []})
    # Automatic correction (region-prep-31): new and edited art corrected, opened art, selected art,
    # locked and unstroked art left alone, and art the user took back left alone after a move.
    out.append({"id": "auto", "kind": "auto", "runner": "run-ai-auto.ps1", "args": []})
    out.append({"id": "p32", "kind": "p32", "runner": "run-ai-p32.ps1", "args": []})
    out.append({"id": "holes", "kind": "holes", "runner": "run-ai-owner-file.ps1",
                "args": ["-Script", "ai-holes.jsx", "-Extra", "@tools/jsx/ai-holes-cert.json"]})
    out.append({"id": "refix", "kind": "refix", "font": FONTS[0], "runner": "run-ai-route-refix.ps1", "args": ["-Font", FONTS[0]]})
    out.append({"id": "exports", "kind": "exports", "font": FONTS[0], "runner": "run-ai-route-exports.ps1", "args": ["-Font", FONTS[0]]})
    out.append({"id": "census", "kind": "census", "runner": "run-ai-fresh.ps1", "args": ["-Script", "ai-coverage-census.jsx"]})
    return out


def receipt(out, job):
    return out / job["id"] / f"{job['id']}-host.txt"


def run(args):
    out = (ROOT / args.out).resolve() if not Path(args.out).is_absolute() else Path(args.out)
    start = time.time()
    for job in jobs():
        if args.only and not fnmatch.fnmatch(job["id"], args.only):
            continue
        if receipt(out, job).exists():
            continue
        if args.budget and time.time() - start > args.budget:
            print("budget spent; run again to continue")
            return 0
        folder = out / job["id"]
        folder.mkdir(parents=True, exist_ok=True)
        cmd = [sys.executable, str(HIDDEN), "--cwd", str(ROOT), "--timeout", "580", "--",
               "powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(ROOT / "tools" / job["runner"]),
               "-OutDir", str(folder.relative_to(ROOT)), "-RunId", job["id"], *job["args"]]
        if job["kind"] not in ("census", "panel"):
            cmd += ["-ExpectedBuild", args.build]
        # Never the user's panel switches: each job gets a settings file of its own (defaults until
        # a job writes one), which Illustrator inherits (IllustratorSettings.h). A run on 2026-10-30
        # read the owner's file with the master switch off and failed six lines.
        env = dict(os.environ, STROKEMASTER_SETTINGS=str(folder / f"{job['id']}-settings.txt"))
        t = time.time()
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=600, env=env)
        tail = (result.stdout or "").strip().splitlines()[-1:] or [""]
        print(f"{job['id']}: {round(time.time() - t)} s, {tail[0][:120]}")
    return 0


def measure_story(folder, job):
    meas = measure_case(folder, job["id"], job["case"])
    rec = json.loads((folder / f"{job['id']}-results.json").read_text(encoding="utf-8-sig"))
    row = rec["rows"][0]
    route = row.get("route", "")
    gate = route.split('"appearance":{"gate":"', 1)[1].split('"', 1)[0] if '"appearance":{"gate":"' in route else "?"
    out = {"gate": gate, "damage": meas["routeDamagePt2"], "native": meas["nativeDamagePt2"],
           "halo": meas["haloChangedPt2"], "recolor": meas["recolorDamagePt2"],
           "text": [meas["routeCharacters"], meas["sourceCharacters"]],
           "glyphs": [glyphs(folder / f"{job['id']}-{job['case']}-route.pdf"), glyphs(folder / f"{job['id']}-{job['case']}-fill.pdf")]}
    # After native-fix as well (the user's script runs both): the miter limits.
    if "fixedDamagePt2" in meas:
        out["fixed"] = {"damage": meas["fixedDamagePt2"], "halo": meas["fixedHaloChangedPt2"]}
    return out


def measure_runs(folder, job):
    k = job["case"]
    row = json.loads((folder / f"{job['id']}-results.json").read_text(encoding="utf-8-sig"))["rows"][0]
    f, ft = render(folder / f"{job['id']}-{k}-fill.pdf")
    n, _ = render(folder / f"{job['id']}-{k}-native.pdf")
    r, rt = render(folder / f"{job['id']}-{k}-route.pdf")
    core = core_of(f)
    outside = ~ndimage.binary_dilation(f.min(axis=2) < INK, iterations=ERODE)
    return {"gate": row.get("gate"), "order": [row.get("before"), row.get("after")], "groups": row.get("groups"),
            "damage": round(int((core & differs(r, f)).sum()) * PT2, 2), "native": round(int((core & differs(n, f)).sum()) * PT2, 2),
            "halo": round(int((outside & differs(r, n)).sum()) * PT2, 2), "text": [len(rt), len(ft)],
            "glyphs": [glyphs(folder / f"{job['id']}-{k}-route.pdf"), glyphs(folder / f"{job['id']}-{k}-fill.pdf")]}


def measure_refix(folder, job):
    p = folder / job["id"]
    fill, ft = render(Path(f"{p}-fill.pdf"))
    fr, _ = render(Path(f"{p}-fill-recolor.pdf"))
    route, rt = render(Path(f"{p}-route.pdf"))
    rec, rct = render(Path(f"{p}-recolor.pdf"))
    core, rcore = core_of(fill), core_of(fr)
    px = lambda pt: int(pt * 600 / 72)
    bands = {"A": (0, 240), "B": (240, 480), "C": (480, 660), "D": (660, 840), "E": (840, 1100)}
    out = {"text": [len(rt), len(rct), len(ft)],
           "glyphs": [glyphs(Path(f"{p}-route.pdf")), glyphs(Path(f"{p}-recolor.pdf")), glyphs(Path(f"{p}-fill.pdf"))]}
    for k, (a, b) in bands.items():
        sl = slice(px(a), px(b))
        out[k] = {"damage": round(int((core[sl] & differs(route[sl], fill[sl])).sum()) * PT2, 2),
                  "recolor": round(int((rcore[sl] & differs(rec[sl], fr[sl])).sum()) * PT2, 2)}
    return out


def measure_exports(folder, job):
    import pymupdf
    from PIL import Image
    p = folder / job["id"]
    out = {}
    for ext in ["pdf", "png", "pdfx4.pdf", "pdfx1a.pdf", "jpg"]:
        if not Path(f"{p}-fill.{ext}" if "." not in ext else f"{p}-fill-{ext}").exists():
            continue

        def load(kind):
            path = Path(f"{p}-{kind}.{ext}" if "." not in ext else f"{p}-{kind}-{ext}")
            if ext in ("png", "jpg"):
                return np.asarray(Image.open(path).convert("RGB")).astype(np.int16)
            pix = pymupdf.open(path)[0].get_pixmap(dpi=300, alpha=False)
            return np.frombuffer(pix.samples, np.uint8).reshape(pix.height, pix.width, pix.n)[:, :, :3].astype(np.int16)
        f, n, r = load("fill"), load("native"), load("route")
        core = ndimage.binary_erosion(f.min(axis=2) < 160, iterations=2)
        if ext.endswith(".pdf"):
            _, xt = render(Path(f"{p}-route-{ext}"))
            out[ext + ".text"] = len(xt)
            out[ext + ".glyphs"] = glyphs(Path(f"{p}-route-{ext}"))
        out[ext] = {"damage": round(int((core & differs(r, f)).sum()) * (72 / 300) ** 2, 2),
                    "native": round(int((core & differs(n, f)).sum()) * (72 / 300) ** 2, 2)}
    _, ft = render(Path(f"{p}-fill.pdf"))
    _, rt = render(Path(f"{p}-route.pdf"))
    out["text"] = [len(rt), len(ft)]
    out["glyphs"] = [glyphs(Path(f"{p}-route.pdf")), glyphs(Path(f"{p}-fill.pdf"))]
    return out


def measure_census(folder, job):
    subprocess.run([sys.executable, str(ROOT / "tools" / "measure_coverage_census.py"), str(folder), "--out",
                    str(folder / "measure.json")], check=True, capture_output=True)
    data = json.loads((folder / "measure.json").read_text(encoding="utf-8"))
    rows = {}
    for row in data["rows"]:
        for part, v in row.items():
            if isinstance(v, dict) and "corrected" in v:
                rows[f"{row['row']}.{part}"] = {k: round(v["corrected"][k], 2) for k in
                                                ("escaping", "stray", "holes", "points_missing", "spill")}
    return {"rows": rows}


def measure_panel(folder, job):
    import re
    r = json.loads((folder / f"{job['id']}-results.json").read_text(encoding="utf-8-sig"))
    def num(text, key):
        m = re.search(f'"{key}":([a-z0-9]+)', text or "")
        return m.group(1) if m else "?"
    p = folder / job["id"]
    f, ft = render(Path(f"{p}-fill.pdf"))
    n, _ = render(Path(f"{p}-native.pdf"))
    rt_img, rt = render(Path(f"{p}-route.pdf"))
    core = core_of(f)
    return {"panel": num(r["status0"], "panel"), "menu": num(r["status0"], "menu"), "rows": num(r["status0"], "rows"),
            "storiesToggled": "text_stories=1" in r["settings1"], "restored": "text_stories=0" in r["settings2"],
            "masterOffEnabled": num(r["statusMasterOff"], "enabled"), "masterOffCorrect": num(r["statusMasterOff"], "correctEnabled"),
            "groups": r["groups"], "framesInGroup": r["framesInGroup"], "groupsAfterUndo": r["groupsAfterUndo"],
            "settingsLeft": not r.get("settingsRemoved", False) and not r.get("settingsExisted", False),
            "damage": round(int((core & differs(rt_img, f)).sum()) * PT2, 2), "text": [len(rt), len(ft)],
            "glyphs": [glyphs(Path(f"{p}-route.pdf")), glyphs(Path(f"{p}-fill.pdf"))]}


def measure_switches(folder, job):
    import re
    from measure_correct_switches import measure
    m = measure(folder, job["id"])

    def num(text, key):
        found = re.search(f'"{key}":(-?[0-9]+)', text or "")
        return int(found.group(1)) if found else None
    out = {"tips": num(m.get("uiStatusAfterTab"), "tips"), "tabTo": num(m.get("uiStatusAfterTab"), "tabTo"),
           "square": [m.get("squareBefore"), m.get("squareOn"), m.get("squareOff")], "routeLimit": m.get("routeLimit"),
           "glyphs": [m["stages"]["dashes-off"]["glyphsDrawn"], m["fillGlyphsDrawn"]], "stages": {}}
    for name, st in m["stages"].items():
        if name == "native":
            continue
        c = st["correct"]
        out["stages"][name] = {"damage": st["damagePt2"], "groups": st["groups"], "gate": c["gate"],
                               **{k: c[k] for k in ("stories", "frames", "paths", "unrouted", "dissolved", "undashed")}}
    return out


def measure_auto(folder, job):
    import re
    d = json.loads((folder / f"{job['id']}-results.json").read_text(encoding="utf-8-sig"))

    # Text: routed or not. Paths: the miter limit as text, so a limit lowered fails as one raised
    # does (a float is only judged against a rise).
    def items(block):
        return {k: {"routed": bool(v.get("routed"))} if k.endswith("text") else {"limit": f"{v.get('limit')}"} for k, v in block.items()}

    def counts(text):
        return {k: int(re.search(f'"{k}":([0-9]+)', text).group(1)) for k in ("automatic", "released", "skipped")}
    return {"afterEdit": items(d["phase3"]["items"]), "afterMoves": items(d["afterIdle"]),
            "takenBack": f"{d['phase4']['released'].get('limit')}", "refix": counts(d["refix"])}


def measure_p32(folder, job):
    """The owner's prep-31 retests as host tests (region-prep-32): an Appearance stroke on text,
    threaded text grouped automatically, a dashed Outside square set Inside, a limit set by hand."""
    import re
    p = {n: json.loads((folder / f"{job['id']}-phase{n}.json").read_text(encoding="utf-8-sig")) for n in (1, 3, 4)}

    def found(pattern, text):
        m = re.search(pattern, text)
        return m.group(1) if m else ""

    obj3, obj4 = p[3]["objText"]["dump"], p[4]["objText"]["dump"]
    return {"squareDoubledWidth": f"{p[1]['squareWidth']}",
            "objectStrokeOrder": found(r"order '([^']*)'", obj3),
            "objectStrokeLimit": found(r"STROKE w 7 join 0 limit ([0-9.]+)", obj3),
            "threadParents": [str(x) for x in p[3]["threadParents"]],
            "squareInside": "StrokeOffsetInside=T" in p[3]["square"]["dump"],
            "squareWidth": f"{p[3]['squareWidth']}",
            "takenBackLimit": found(r"STROKE w 7 join 0 limit ([0-9.]+)", obj4),
            "takenBackReleased": found(r"RECORD marked [01] released ([01])", obj4),
            "refix": {k: int(found(f'"{k}":([0-9]+)', p[4]["refix"]) or -1) for k in ("automatic", "released", "threaded")}}


def measure_holes(folder, job):
    """Close holes (region-prep-33): per font, whether the effect's fill is on the copy corrected with the
    switch on and off the others, and whether the effect published every silhouette it was asked for."""
    import re
    d = json.loads((folder / f"{job['id']}-holes.json").read_text(encoding="utf-8-sig"))
    out = {"fonts": sorted(d["fonts"].keys())}
    for name, row in d["fonts"].items():
        out[name] = {k: "close-holes.v1" in row[k]["dump"] for k in ("off", "on", "off-again")}
    m = re.search(r'"passed":([0-9]+).*"gate":"([^"]*)"', d.get("effect", ""))
    out["effect"] = {"passed": int(m.group(1)) if m else -1, "gate": m.group(2) if m else ""}
    return out


MEASURES = {"holes": measure_holes, "p32": measure_p32, "auto": measure_auto, "switches": measure_switches, "panel": measure_panel, "story": measure_story, "runs": measure_runs, "refix": measure_refix, "exports": measure_exports, "census": measure_census}


def measure(args):
    out = Path(args.out)
    cert = {"schema": 1, "jobs": {}, "missing": []}
    for job in jobs():
        folder = out / job["id"]
        if not receipt(out, job).exists():
            cert["missing"].append(job["id"])
            continue
        host = receipt(out, job).read_text(encoding="utf-8-sig", errors="replace")
        try:
            cert["jobs"][job["id"]] = MEASURES[job["kind"]](folder, job)
            cert["jobs"][job["id"]]["host"] = host.split("|")[0]
        except Exception as e:  # a failed job is a failed line, not a crash of the runner
            cert["jobs"][job["id"]] = {"error": f"{type(e).__name__}: {e}", "hostTail": host[-200:]}
    (out / "certification.json").write_text(json.dumps(cert, indent=2) + "\n", encoding="utf-8")
    print(f"measured {len(cert['jobs'])} jobs, missing {len(cert['missing'])}")
    return 0


def compare(args):
    out = Path(args.out)
    cert = json.loads((out / "certification.json").read_text(encoding="utf-8"))
    if args.write_baseline:
        Path(args.baseline).write_text(json.dumps(cert, indent=2) + "\n", encoding="utf-8")
        print(f"baseline written: {args.baseline}")
        return 0
    base = json.loads(Path(args.baseline).read_text(encoding="utf-8"))
    fails = []

    def num(path, a, b, halo=False):
        slack = max(2.0, 0.10 * a) if halo else max(1.0, 0.05 * a)
        if b > a + slack:
            fails.append(f"{path}: {a} -> {b}")

    def walk(path, a, b):
        if isinstance(a, dict):
            for k, v in a.items():
                if k in ("host", "native", "hostTail", "order") or k == "error":
                    continue
                if k not in b:
                    fails.append(f"{path}.{k}: missing")
                    continue
                walk(f"{path}.{k}", v, b[k])
        elif isinstance(a, (list, str, bool)) or isinstance(b, bool):
            # Exact: a boolean is a number to Python, and a flipped check must not pass as "smaller".
            if a != b:
                fails.append(f"{path}: {a} -> {b}")
        elif isinstance(a, int) and isinstance(b, int):
            # Counts (groups, frames, rows) are exact; measured areas are floats.
            if a != b:
                fails.append(f"{path}: {a} -> {b}")
        elif isinstance(a, (int, float)) and isinstance(b, (int, float)):
            num(path, a, b, halo=path.endswith(".halo"))
    for jid, a in base["jobs"].items():
        b = cert["jobs"].get(jid)
        if b is None:
            fails.append(f"{jid}: not run")
        elif "error" in b:
            fails.append(f"{jid}: {b['error']}")
        else:
            walk(jid, a, b)
    for line in fails:
        print("FAIL", line)
    print(f"{'FAIL' if fails else 'PASS'}: {len(base['jobs'])} jobs against {args.baseline}, {len(fails)} failed lines")
    return 1 if fails else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("plan")
    r = sub.add_parser("run")
    r.add_argument("--out", required=True)
    r.add_argument("--build", required=True)
    r.add_argument("--only")
    r.add_argument("--budget", type=float, default=0)
    m = sub.add_parser("measure")
    m.add_argument("--out", required=True)
    c = sub.add_parser("compare")
    c.add_argument("--out", required=True)
    c.add_argument("--baseline", required=True)
    c.add_argument("--write-baseline", action="store_true")
    args = ap.parse_args()
    if args.cmd == "plan":
        for job in jobs():
            print(job["id"], job["runner"], " ".join(job["args"]))
        print(f"{len(jobs())} jobs")
        return 0
    return {"run": run, "measure": measure, "compare": compare}[args.cmd](args)


if __name__ == "__main__":
    raise SystemExit(main())
