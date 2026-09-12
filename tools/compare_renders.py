"""Compare two renders of one document, frame by frame.

Usage:
    python tools/compare_renders.py <a.png> <b.png> <manifest.json> <out-stem> [--ppi 300]

A and B must be renders of the same document at the same resolution: the intended use is one
export made with the plugin loaded and one made without it, from the same saved .indd, so that
the renderer is the only variable.

Without --fill-mask, this measures eroded non-background ink, including strokes and effects.
That mask is NOT a glyph-fill mask and cannot establish fill compositing correctness.
An independent grayscale fill-only mask (white=fill coverage, black=outside) can be supplied
at the same pixel size. Only full-coverage pixels survive thresholding and erosion. A mask
derived from either compared render is not an independent reference. Differences outside
the core are reported without labeling them harmless. These statistics do not prove that
the chosen reference itself represents the intended appearance.
"""
import json
import argparse
import os
import sys

import numpy as np
from PIL import Image

try:
    from scipy import ndimage
except ImportError:
    ndimage = None

DIFF = 8          # per-channel difference that counts as a changed pixel
ERODE_PT = 0.75   # how far inside the glyph edge the "interior" begins
FG = 12           # per-channel difference from the backdrop that counts as ink

# How far outside a frame to look. An Outside stroke reaches its full weight beyond the frame,
# so the crop has to be wider than the frame itself, but a crop that reaches a neighboring
# frame measures that neighbor instead: with 8 pt strokes on rows 24 pt apart, a 24 pt pad
# takes in 8 pt of the next row's stroke and reports its repaint as this frame's change. That
# is exactly what made two frames with no repaint path of their own appear to change.
PAD_X = 12.0
PAD_Y = 12.0


def lum(a):
    return 0.299 * a[:, :, 0] + 0.587 * a[:, :, 1] + 0.114 * a[:, :, 2]


def crop(img, bounds, pad_x, pad_y, scale):
    y1, x1, y2, x2 = bounds
    if not all(np.isfinite(v) for v in bounds) or y2 < y1 or x2 < x1:
        raise ValueError("invalid or reversed frame bounds")
    h, w = img.shape[:2]
    box = (min(w, max(0, int((x1 - pad_x) * scale))), min(h, max(0, int((y1 - pad_y) * scale))),
           max(0, min(w, int(round((x2 + pad_x) * scale)))), max(0, min(h, int(round((y2 + pad_y) * scale)))))
    return img[box[1]:box[3], box[0]:box[2]]


def interior_mask(ink, erode):
    if ndimage is None:
        raise RuntimeError("scipy is required; refusing an uneroded fallback")
    return ndimage.binary_erosion(ink, iterations=erode)


def opaque_image(path, mode):
    with Image.open(path) as image:
        if ("A" in image.getbands() or "transparency" in image.info) and np.any(np.asarray(image.convert("RGBA"))[:, :, 3] != 255):
            raise ValueError("transparent inputs are unsupported; composite against an explicit common backdrop first")
        return np.asarray(image.convert(mode)).copy()


def main(a_path, b_path, manifest_path, out_stem, ppi=300.0, fill_mask_path=None):
    a_img = opaque_image(a_path, "RGB").astype(int)
    b_img = opaque_image(b_path, "RGB").astype(int)
    if a_img.shape != b_img.shape:
        raise SystemExit("renders differ in size: %s vs %s" % (a_img.shape, b_img.shape))
    if not np.isfinite(ppi) or ppi <= 0:
        raise ValueError("ppi must be finite and positive")
    fill_mask = None
    if fill_mask_path:
        fill_mask = opaque_image(fill_mask_path, "L") >= 254
        if fill_mask.shape != a_img.shape[:2]:
            raise ValueError("fill mask and renders differ in size")
    with open(manifest_path, encoding="utf-8") as source:
        manifest = json.load(source)
    scale = ppi / 72.0
    erode = max(1, int(round(ERODE_PT * scale)))

    whole = (np.abs(a_img - b_img).max(axis=2) > DIFF)
    report = {"a": os.path.basename(a_path), "b": os.path.basename(b_path), "ppi": ppi,
              "erode_px": erode, "page_changed_px": int(whole.sum()), "frames": [],
              "schema": 2,
              "mask_kind": "external_fill_only" if fill_mask is not None else "estimated_nonbackground_ink",
              "fill_compositing_verified": False,
              "mask_file": os.path.basename(fill_mask_path) if fill_mask_path else None}

    for fr in manifest["frames"]:
        name = fr.get("name") or "item"
        rec = {"key": fr.get("key") or name.split(" ")[0], "name": name,
               "palette": fr.get("palette"), "note": fr.get("note")}
        ca = crop(a_img, fr["bounds"], PAD_X, PAD_Y, scale)
        cb = crop(b_img, fr["bounds"], PAD_X, PAD_Y, scale)
        if ca.size == 0:
            report["frames"].append(rec)
            continue
        diff = np.abs(ca - cb).max(axis=2)
        changed = diff > DIFF
        rec["changed_px"] = int(changed.sum())
        rec["max_channel_diff"] = int(diff.max())

        # Median-color backdrop is a heuristic. Dense ink can dominate it; only an
        # independent coverage mask supports measurements specifically about glyph fills.
        backdrop = np.median(ca.reshape(-1, 3), axis=0)
        ink = (np.abs(ca - backdrop).max(axis=2) > FG)
        coverage = crop(fill_mask, fr["bounds"], PAD_X, PAD_Y, scale) if fill_mask is not None else ink
        interior = interior_mask(coverage, erode)
        rec["ink_px"] = int(ink.sum())
        rec["interior_px"] = int(interior.sum())
        rec["interior_changed_px"] = int((changed & interior).sum())
        rec["interior_nonzero_px"] = int(((diff > 0) & interior).sum())
        rec["interior_max_channel_diff"] = int(diff[interior].max()) if interior.any() else None
        rec["edge_changed_px"] = int((changed & ~interior).sum())
        if interior.any():
            la, lb = lum(ca)[interior], lum(cb)[interior]
            rec["interior_lum_a"] = round(float(la.mean()), 2)
            rec["interior_lum_b"] = round(float(lb.mean()), 2)
            rec["interior_dlum"] = round(float(lb.mean() - la.mean()), 3)
            moved = changed & interior
            if moved.any():
                rec["interior_changed_dlum"] = round(float((lum(cb)[moved] - lum(ca)[moved]).mean()), 2)
        report["frames"].append(rec)

    with open(out_stem + ".json", "w", encoding="utf-8") as f:
        json.dump(report, f, indent=1)

    md = ["Mask: " + report["mask_kind"] + ". These statistics alone do not certify compositing correctness.", "",
          "| Entry | Variant | Changed px | Worst channel | Mask interior changed | Interior dLum | Interior lum A -> B |",
          "| --- | --- | --- | --- | --- | --- | --- |"]
    for r in report["frames"]:
        md.append("| %s | %s | %s | %s | %s | %s | %s |" % (
            r.get("key", ""), (r.get("name") or "").split(" ", 1)[-1],
            r.get("changed_px", ""), r.get("max_channel_diff", ""),
            r.get("interior_changed_px", ""), r.get("interior_dlum", ""),
            "%s -> %s" % (r.get("interior_lum_a", "?"), r.get("interior_lum_b", "?"))))
    text = "\n".join(md) + "\n\nWhole page changed pixels: %d (erosion %d px at %g ppi)\n" % (
        report["page_changed_px"], erode, ppi)
    with open(out_stem + ".md", "w", encoding="utf-8") as f:
        f.write(text)
    print(text)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("a", "b", "manifest", "out_stem"):
        parser.add_argument(name)
    parser.add_argument("--ppi", type=float, default=300.0)
    parser.add_argument("--fill-mask", help="Independent white-on-black glyph-fill mask at render size")
    args = parser.parse_args()
    main(args.a, args.b, args.manifest, args.out_stem, args.ppi, args.fill_mask)
