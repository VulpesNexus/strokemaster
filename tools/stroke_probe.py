"""Probe the PDFs written by the tools/jsx build scripts.

Usage:
    python tools/stroke_probe.py <actual-dir> <stem>

Reads <stem>.pdf and <stem>.manifest.json from <actual-dir>. Writes <stem>.probe.json,
<stem>.probe.md, crops/<stem>-<frame>.png, and the edited PDFs used for measurement into
<actual-dir>/probe/.

For every manifest entry it reports the paint units on each line (text objects in paint
order: S stroke, F fill, FS fill and stroke in one object), an estimate of the wax runs they
come from, the stroke state emitted, and the paths painted inside the entry's bounds.

Measurements always work on an isolated copy of the page: every text object and path that
does not belong to the entry is removed first (the page backdrop stays), so neighboring
frames cannot contaminate the numbers. Text inside transparency-group forms is handled.
    hidden  core fill pixels covered by white stroke, for the entry's group, as exported and
            with all of the group's strokes painted before all of its fills
    spikes  white pixels farther from the glyph fill than the stroke weight, and the largest
            reach of the stroke from the fill
    apex    how far the stroke extends above a wedge's apex, with the textbook miter, bevel,
            and round distances for comparison
    pair    live text against its outlined copy 290 pt to the right
Rendering is MuPDF at 720 dpi, so one point is exactly ten pixels.
"""
import json
import math
import os
import re
import sys

import numpy as np
import pikepdf
import pymupdf
from pikepdf import Name, Operator
from PIL import Image

try:
    from scipy import ndimage
except ImportError:  # the spikes measurement needs SciPy
    ndimage = None

DPI = 720
PX = DPI / 72.0
STATE_OPS = ("w", "j", "J", "M", "d", "Tc", "Tw", "Tz", "TL", "Ts", "Tr", "Tf")
DEFAULTS = {"w": [1], "j": [0], "J": [0], "M": [10], "d": [pikepdf.Array([]), 0], "Tc": [0], "Tw": [0], "Tz": [100], "TL": [0], "Ts": [0], "Tr": [0]}
SHOWS = ("TJ", "Tj", "'", '"')
PAINTS = ("S", "s", "f", "F", "f*", "B", "B*", "b", "b*", "n")
STROKE_TO_FILL = {"S": "n", "s": "n", "B": "f", "B*": "f*", "b": "f", "b*": "f*"}
IDENT = (1.0, 0.0, 0.0, 1.0, 0.0, 0.0)


def mul(a, b):
    return (a[0] * b[0] + a[1] * b[2], a[0] * b[1] + a[1] * b[3], a[2] * b[0] + a[3] * b[2], a[2] * b[1] + a[3] * b[3], a[4] * b[0] + a[5] * b[2] + b[4], a[4] * b[1] + a[5] * b[3] + b[5])


def apply(m, x, y):
    return (m[0] * x + m[2] * y + m[4], m[1] * x + m[3] * y + m[5])


def num(v):
    return float(v)


def snapshot(gs):
    s = dict(gs)
    for k in ("cm", "gsn", "sc", "fc"):
        s[k] = list(gs[k])
    return s


def opval(entry, default=None):
    return num(entry[0][0]) if entry else default


def analyze(ops, resources=None, base_ctm=IDENT, form=None, pdf_height=0.0, out=None):
    """Collect text blocks and path paints, with the graphics state that applies to each.

    Indices ("start", "shows", "index") are positions in the operator list of the stream the
    object lives in; "form" names that stream ("/Fm0", "/Fm0/Fm1") or is None for the page."""
    if out is None:
        out = {"blocks": [], "paints": []}
    gs = {k: None for k in STATE_OPS}
    gs.update({"cm": [], "gsn": [], "sc": [], "fc": [], "ctm": base_ctm})
    stack, cur, path = [], None, []
    for idx, (operands, op) in enumerate(ops):
        o = str(op)
        if o == "q":
            stack.append(snapshot(gs))
        elif o == "Q":
            gs = stack.pop() if stack else gs
        elif o == "cm":
            gs["cm"] = gs["cm"] + [(operands, op)]
            gs["ctm"] = mul(tuple(num(x) for x in operands), gs["ctm"])
        elif o == "gs":
            gs["gsn"] = gs["gsn"] + [(operands, op)]
        elif o in STATE_OPS:
            gs[o] = (operands, op)
        elif o in ("G", "RG", "K", "CS"):
            gs["sc"] = [(operands, op)]
        elif o in ("SC", "SCN"):
            gs["sc"] = gs["sc"] + [(operands, op)]
        elif o in ("g", "rg", "k", "cs"):
            gs["fc"] = [(operands, op)]
        elif o in ("sc", "scn"):
            gs["fc"] = gs["fc"] + [(operands, op)]
        if o == "BT":
            cur = {"start": idx, "state": snapshot(gs), "inside": [], "shows": [], "form": form}
            continue
        if cur is not None:
            if o == "ET":
                tr = int(opval(cur["state"]["Tr"], 0))
                origin = None
                st = cur["state"]
                w, j, m = opval(st["w"], 1.0), opval(st["j"], 0), opval(st["M"], 10.0)
                for operands2, op2 in cur["inside"]:
                    s2 = str(op2)
                    if s2 == "Tr":
                        tr = int(num(operands2[0]))
                    elif s2 == "Tm" and origin is None:
                        origin = apply(st["ctm"], num(operands2[4]), num(operands2[5]))
                    elif s2 == "w":
                        w = num(operands2[0])
                    elif s2 == "j":
                        j = num(operands2[0])
                    elif s2 == "M":
                        m = num(operands2[0])
                cur.update({"end": idx, "role": {0: "F", 1: "S", 2: "FS", 3: "I"}.get(tr, "T%d" % tr), "w": w, "j": j, "M": m,
                            "origin_td": (origin[0], pdf_height - origin[1]) if origin else None})
                out["blocks"].append(cur)
                cur = None
            else:
                cur["inside"].append((operands, op))
                if o in SHOWS:
                    cur["shows"].append(idx)
            continue
        if o in ("m", "l"):
            path.append(apply(gs["ctm"], num(operands[0]), num(operands[1])))
        elif o in ("c", "v", "y"):
            vals = [num(x) for x in operands]
            for i in range(0, len(vals), 2):
                path.append(apply(gs["ctm"], vals[i], vals[i + 1]))
        elif o == "re":
            x, y, w_, h_ = [num(x) for x in operands]
            for px, py in ((x, y), (x + w_, y), (x, y + h_), (x + w_, y + h_)):
                path.append(apply(gs["ctm"], px, py))
        elif o in PAINTS:
            if path and o != "n":
                xs = [p[0] for p in path]
                ys = [pdf_height - p[1] for p in path]
                out["paints"].append({"index": idx, "op": o, "form": form, "w": opval(gs["w"], 1.0), "j": opval(gs["j"], 0), "M": opval(gs["M"], 10.0),
                                      "bbox_td": (min(xs), min(ys), max(xs), max(ys)), "points": len(path)})
            path = []
        elif o == "Do" and resources is not None and "/XObject" in resources:
            xo = resources["/XObject"].get(operands[0])
            if xo is not None and xo.get("/Subtype") == "/Form":
                fm = tuple(num(x) for x in xo.get("/Matrix", [1, 0, 0, 1, 0, 0]))
                analyze(list(pikepdf.parse_content_stream(xo)), xo.get("/Resources", resources), mul(fm, gs["ctm"]), (form or "") + str(operands[0]), pdf_height, out)
    return out


def gsreset():
    return pikepdf.Dictionary(Type=Name.ExtGState, CA=1, ca=1, BM=Name.Normal, SMask=Name("/None"), OP=False, op=False, OPM=0, AIS=False)


def emit(block):
    s = block["state"]
    out = [([], Operator("q")), ([Name("/GSreset")], Operator("gs"))]
    out.extend(s["cm"])
    out.extend(s["gsn"])
    for k in STATE_OPS:
        if s[k] is not None:
            out.append(s[k])
        elif k in DEFAULTS:
            out.append((DEFAULTS[k], Operator(k)))
    out.extend(s["sc"] or [([0], Operator("G"))])
    out.extend(s["fc"] or [([0], Operator("g"))])
    out.append(([], Operator("BT")))
    out.extend(block["inside"])
    out.append(([], Operator("ET")))
    out.append(([], Operator("Q")))
    return out


def resolve(pdf, form):
    page = pdf.pages[0]
    if not form:
        return page, page.Resources
    res, obj = page.Resources, None
    for name in re.findall(r"/[^/]+", form):
        obj = res["/XObject"][Name(name)]
        res = obj.get("/Resources", res)
    return obj, res


def edit_pdf(src, dst, drop, paint_to, insert, tr0):
    pdf = pikepdf.open(src)
    forms = {k[0] for k in drop} | {k[0] for k in paint_to} | {k[0] for k in insert} | {k[0] for k in tr0}
    for form in forms:
        obj, res = resolve(pdf, form)
        if "/ExtGState" not in res:
            res[Name.ExtGState] = pikepdf.Dictionary()
        res["/ExtGState"][Name("/GSreset")] = gsreset()
        ops = list(pikepdf.parse_content_stream(obj))
        new = []
        for i, x in enumerate(ops):
            key = (form, i)
            if key in insert:
                new.extend(insert[key])
            if key in drop:
                continue
            if key in tr0:
                new.append(([0], Operator("Tr")))
            if key in paint_to:
                new.append(([], Operator(paint_to[key])))
                continue
            new.append(x)
        data = pikepdf.unparse_content_stream(new)
        if form is None:
            pdf.pages[0].Contents = pdf.make_stream(data)
        else:
            obj.write(data)
    pdf.save(dst)


def is_backdrop(p, page_wh):
    x1, y1, x2, y2 = p["bbox_td"]
    return (x2 - x1) >= 0.95 * page_wh[0] and (y2 - y1) >= 0.95 * page_wh[1]


def overlaps(b, box):
    return b[0] < box[2] and b[2] > box[0] and b[1] < box[3] and b[3] > box[1]


def make_edits(blocks, paints, sel_blocks, sel_paints, mode, page_wh):
    """mode: isolate | fills | strokes_first. All modes remove everything outside the selection."""
    sb = {(b["form"], b["start"]) for b in sel_blocks}
    sp = {(p["form"], p["index"]) for p in sel_paints}
    drop, paint_to, insert, tr0 = set(), {}, {}, set()
    for b in blocks:
        if (b["form"], b["start"]) not in sb:
            drop.update((b["form"], i) for i in b["shows"])
    for p in paints:
        if (p["form"], p["index"]) not in sp and not is_backdrop(p, page_wh):
            paint_to[(p["form"], p["index"])] = "n"
    if mode == "fills":
        for b in sel_blocks:
            if b["role"] == "S":
                drop.update((b["form"], i) for i in b["shows"])
            elif b["role"] == "FS":
                tr0.update((b["form"], i) for i in b["shows"])
        for p in sel_paints:
            if p["op"] in STROKE_TO_FILL:
                paint_to[(p["form"], p["index"])] = STROKE_TO_FILL[p["op"]]
    elif mode == "strokes_first":
        by_form = {}
        for b in sel_blocks:
            by_form.setdefault(b["form"], []).append(b)
        for form, bs in by_form.items():
            bs.sort(key=lambda b: b["start"])
            for b in bs:
                drop.update((form, i) for i in b["shows"])
            ops = []
            for b in [b for b in bs if b["role"] == "S"] + [b for b in bs if b["role"] != "S"]:
                ops.extend(emit(b))
            insert[(form, bs[0]["start"])] = ops
    return drop, paint_to, insert, tr0


def variant(pdf_path, probe_dir, tag, blocks, paints, sel_b, sel_p, mode, page_wh):
    dst = os.path.join(probe_dir, "%s-%s.pdf" % (tag, mode))
    edit_pdf(pdf_path, dst, *make_edits(blocks, paints, sel_b, sel_p, mode, page_wh))
    return dst


def render(pdf_path, box_td):
    x1, y1, x2, y2 = box_td
    doc = pymupdf.open(pdf_path)
    pix = doc[0].get_pixmap(dpi=DPI, clip=pymupdf.Rect(x1, y1, x2, y2), alpha=False)
    return np.frombuffer(pix.samples, dtype=np.uint8).reshape(pix.height, pix.width, pix.n)[:, :, :3].astype(int)


def lum(a):
    return 0.299 * a[:, :, 0] + 0.587 * a[:, :, 1] + 0.114 * a[:, :, 2]


def frame_box(fr, pad_x, pad_y=None):
    y1, x1, y2, x2 = fr["bounds"]
    pad_y = pad_x if pad_y is None else pad_y
    return (x1 - pad_x, y1 - pad_y, x2 + pad_x, y2 + pad_y)


def union_box(boxes):
    return (min(b[0] for b in boxes), min(b[1] for b in boxes), max(b[2] for b in boxes), max(b[3] for b in boxes))


def assign(blocks, frames):
    for b in blocks:
        b["frame"], b["line"] = None, None
        if not b["origin_td"]:
            continue
        x, y = b["origin_td"]
        best = None
        for fi, fr in enumerate(frames):
            for li, ln in enumerate(fr.get("lines") or []):
                if ln["x0"] - 2 <= x <= ln["x1"] + 2:
                    dy = abs(y - ln["baseline"])
                    if dy < 12 and (best is None or dy < best[0]):
                        best = (dy, fi, li)
        if best:
            b["frame"], b["line"] = best[1], best[2]


def estimate_runs(roles):
    runs, prev = 0, None
    for r in roles:
        if r == "FS":
            runs += 1
        elif r == "S" and prev in (None, "F", "FS"):
            runs += 1
        elif r == "F" and prev in (None, "FS"):
            runs += 1
        prev = r
    return runs


def tag_of(fr, fi):
    return re.sub(r"[^A-Za-z0-9._-]+", "_", (fr.get("name") or "item%s" % fr.get("id", fi)).split(" ")[0] + "_%d" % fi)


def main(actual_dir, stem):
    pdf_path = os.path.join(actual_dir, stem + ".pdf")
    manifest = json.load(open(os.path.join(actual_dir, stem + ".manifest.json"), encoding="utf-8"))
    frames = manifest["frames"]
    probe_dir = os.path.join(actual_dir, "probe")
    crop_dir = os.path.join(actual_dir, "crops")
    os.makedirs(probe_dir, exist_ok=True)
    os.makedirs(crop_dir, exist_ok=True)
    pdf = pikepdf.open(pdf_path)
    page = pdf.pages[0]
    page_wh = (float(page.MediaBox[2]), float(page.MediaBox[3]))
    found = analyze(list(pikepdf.parse_content_stream(page)), page.get("/Resources"), IDENT, None, page_wh[1])
    blocks, paints = found["blocks"], found["paints"]
    assign(blocks, frames)
    report = {"pdf": stem + ".pdf", "app": manifest.get("app"), "frames": []}

    def selection(indices):
        sb = [b for b in blocks if b["frame"] in indices]
        boxes = [frame_box(frames[i], 1.0) for i in indices]
        sp = [p for p in paints if not is_backdrop(p, page_wh) and any(overlaps(p["bbox_td"], bx) for bx in boxes)]
        return sb, sp

    for fi, fr in enumerate(frames):
        keep = ("key", "name", "variant", "status", "group", "section", "angle", "align", "weight", "role", "row", "requestedMiterLimit", "readBackMiterLimit", "result", "cls", "strokeAlignment", "strokeWeight")
        rec = {k: fr.get(k) for k in keep if fr.get(k) is not None}
        tag = tag_of(fr, fi)
        sel_b, sel_p = selection([fi])
        lines = []
        for li in range(len(fr.get("lines") or [])):
            roles = [b["role"] for b in sel_b if b["line"] == li]
            lines.append({"runs": estimate_runs(roles), "objects": len(roles), "order": " ".join(roles),
                          "stroke_state": sorted({"w=%g j=%g M=%g" % (b["w"], b["j"], b["M"]) for b in sel_b if b["line"] == li and b["role"] in ("S", "FS")})})
        rec["lines"] = lines
        rec["paths"] = ["%s(w=%g j=%g M=%g pts=%d)" % (p["op"], p["w"], p["j"], p["M"], p["points"]) for p in sel_p]
        measure = fr.get("measure")
        ext = max([b["w"] / 2.0 for b in sel_b if b["role"] in ("S", "FS")] or [float(fr.get("weight") or 0)])

        if measure == "hidden":
            group = [i for i, f in enumerate(frames) if fr.get("group") and f.get("group") == fr.get("group")] or [fi]
            gb, gp = selection(group)
            gext = max([b["w"] / 2.0 for b in gb if b["role"] in ("S", "FS")] or [ext])
            box = union_box([frame_box(frames[i], gext + 2, 2 * gext + 4) for i in group])
            iso = render(variant(pdf_path, probe_dir, tag, blocks, paints, gb, gp, "isolate", page_wh), box)
            fills = render(variant(pdf_path, probe_dir, tag, blocks, paints, gb, gp, "fills", page_wh), box)
            first = render(variant(pdf_path, probe_dir, tag, blocks, paints, gb, gp, "strokes_first", page_wh), box)
            core = lum(fills) < 60
            rec["fill_core_px"] = int(core.sum())
            rec["hidden_fill_px"] = int((core & (lum(iso) > 200)).sum())
            rec["hidden_fill_px_strokes_first"] = int((core & (lum(first) > 200)).sum())
            Image.fromarray(np.vstack([iso, first]).astype(np.uint8)).save(os.path.join(crop_dir, "%s-%s.png" % (stem, tag)))
        elif measure == "spikes" and ndimage is not None and (sel_b or sel_p):
            weight = float(fr.get("weight") or ext)
            box = frame_box(fr, 30.0)
            iso = render(variant(pdf_path, probe_dir, tag, blocks, paints, sel_b, sel_p, "isolate", page_wh), box)
            fills = render(variant(pdf_path, probe_dir, tag, blocks, paints, sel_b, sel_p, "fills", page_wh), box)
            fill = lum(fills) < 128
            white = lum(iso) > 200
            if fill.any() and white.any():
                dist = ndimage.distance_transform_edt(~fill) / PX
                rec["spike_px_beyond_weight"] = int((white & (dist > weight + 0.25)).sum())
                rec["max_stroke_reach_pt"] = round(float(dist[white].max()), 2)
                rec["white_px"] = int(white.sum())
            Image.fromarray(iso.astype(np.uint8)).save(os.path.join(crop_dir, "%s-%s.png" % (stem, tag)))
        elif measure == "apex":
            weight = float(fr.get("weight") or ext)
            ax, ay = fr["apex"]
            box = (ax - 3 * weight - 4, ay - 8 * weight - 4, ax + 3 * weight + 4, ay + 10)
            iso = render(variant(pdf_path, probe_dir, tag, blocks, paints, sel_b, sel_p, "isolate", page_wh), box)
            white = lum(iso) > 200
            rows = np.where(white.any(axis=1))[0]
            if len(rows):
                rec["apex_extension_pt"] = round(ay - (box[1] + rows.min() / PX), 2)
                half = math.radians(fr["angle"]) / 2
                hw = weight if fr.get("align") == "outside" else weight / 2
                rec["theory_pt"] = {"miter": round(hw / math.sin(half), 2), "bevel": round(hw * math.sin(half), 2), "round": round(hw, 2), "miter_ratio": round(1 / math.sin(half), 3)}
        elif measure == "pair" and fr.get("role") == "text":
            live_box = frame_box(fr, 8)
            out_box = (live_box[0] + 290, live_box[1], live_box[2] + 290, live_box[3])
            pb = [b for b in blocks if b["frame"] == fi]
            pp = [p for p in paints if not is_backdrop(p, page_wh) and (overlaps(p["bbox_td"], live_box) or overlaps(p["bbox_td"], out_box))]
            iso_pdf = variant(pdf_path, probe_dir, tag, blocks, paints, pb, pp, "isolate", page_wh)
            live, outl = render(iso_pdf, live_box), render(iso_pdf, out_box)
            if live.shape == outl.shape:
                d = np.abs(live - outl).max(axis=2)
                rec["pair_diff_px"] = int((d > 32).sum())
                rec["pair_white_px"] = [int((lum(live) > 200).sum()), int((lum(outl) > 200).sum())]
                rec["pair_black_px"] = [int((lum(live) < 60).sum()), int((lum(outl) < 60).sum())]
            rec["pair_outline_paths"] = ["%s(w=%g j=%g M=%g pts=%d)" % (p["op"], p["w"], p["j"], p["M"], p["points"]) for p in pp]
            Image.fromarray(np.vstack([live, outl]).astype(np.uint8)).save(os.path.join(crop_dir, "%s-%s.png" % (stem, tag)))
        report["frames"].append(rec)

    with open(os.path.join(actual_dir, stem + ".probe.json"), "w", encoding="utf-8") as f:
        json.dump(report, f, indent=1)
    md = ["| Entry | Variant | Runs per line | Objects in paint order | Stroke state | Paths | Measurement |", "| --- | --- | --- | --- | --- | --- | --- |"]
    for r in report["frames"]:
        meas = {k: r[k] for k in r if k.startswith(("hidden", "spike", "max_", "apex", "theory", "pair_diff", "pair_white", "pair_black", "fill_core", "white_px"))}
        md.append("| %s | %s | %s | %s | %s | %s | %s |" % (
            r.get("name") or "(item)", r.get("variant", ""),
            " / ".join(str(l["runs"]) for l in r["lines"]),
            " / ".join(l["order"] for l in r["lines"]),
            "; ".join(sorted({s for l in r["lines"] for s in l["stroke_state"]})),
            "; ".join(r["paths"][:4]) + (" …" if len(r["paths"]) > 4 else ""),
            json.dumps(meas) if meas else ""))
    with open(os.path.join(actual_dir, stem + ".probe.md"), "w", encoding="utf-8") as f:
        f.write("\n".join(md) + "\n")
    print("\n".join(md))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
