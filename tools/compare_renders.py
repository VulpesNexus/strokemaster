"""Compare two renders of one document, frame by frame.

Usage:
    python tools/compare_renders.py <a.png> <b.png> <manifest.json> <out-stem> [--ppi 300]

A and B must be renders of the same document at the same resolution: the intended use is one
export made with the plugin loaded and one made without it, from the same saved .indd, so that
the renderer is the only variable.

The measurement that matters is not how many pixels changed but *where*. Repainting a run's
fill on top of its own stroke necessarily redraws the fill's outer edge, and antialiasing makes
that edge land a fraction of a pixel differently, so a thin ring of changed pixels around every
stroked glyph is expected and harmless. A fill that composites twice against what is under it,
because it carries a blend mode, a partial opacity or an overprint, changes color across the
whole glyph body instead. Eroding the glyph mask away from its edges separates the two:

    edge_changed        changed pixels within the eroded-away border: expected
    interior_changed    changed pixels in the glyph core: a real appearance change
    interior_dlum       mean luminance of the core in B minus in A, signed, so that a double
                        Multiply reads as negative and a double Screen as positive

A control frame that the adornment is supposed to skip must show interior_changed = 0. A frame
whose only change is the edge ring is corrected without being altered.
"""
import json
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
    h, w = img.shape[:2]
    box = (max(0, int((x1 - pad_x) * scale)), max(0, int((y1 - pad_y) * scale)),
           min(w, int(round((x2 + pad_x) * scale))), min(h, int(round((y2 + pad_y) * scale))))
    return img[box[1]:box[3], box[0]:box[2]]


def main(a_path, b_path, manifest_path, out_stem, ppi=300.0):
    a_img = np.asarray(Image.open(a_path).convert("RGB")).astype(int)
    b_img = np.asarray(Image.open(b_path).convert("RGB")).astype(int)
    if a_img.shape != b_img.shape:
        raise SystemExit("renders differ in size: %s vs %s" % (a_img.shape, b_img.shape))
    manifest = json.load(open(manifest_path, encoding="utf-8"))
    scale = ppi / 72.0
    erode = max(1, int(round(ERODE_PT * scale)))

    whole = (np.abs(a_img - b_img).max(axis=2) > DIFF)
    report = {"a": os.path.basename(a_path), "b": os.path.basename(b_path), "ppi": ppi,
              "erode_px": erode, "page_changed_px": int(whole.sum()), "frames": []}

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

        # The backdrop is the page tint, which is by far the most common color in the crop.
        backdrop = np.median(ca.reshape(-1, 3), axis=0)
        ink = (np.abs(ca - backdrop).max(axis=2) > FG)
        if ndimage is not None and ink.any():
            interior = ndimage.binary_erosion(ink, iterations=erode)
        else:
            interior = ink
        rec["ink_px"] = int(ink.sum())
        rec["interior_px"] = int(interior.sum())
        rec["interior_changed_px"] = int((changed & interior).sum())
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

    md = ["| Entry | Variant | Changed px | Worst channel | Interior changed | Interior dLum | Core lum A -> B |",
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
    argv = [a for a in sys.argv[1:] if not a.startswith("--")]
    ppi = 300.0
    for i, a in enumerate(sys.argv):
        if a == "--ppi":
            ppi = float(sys.argv[i + 1])
    main(argv[0], argv[1], argv[2], argv[3], ppi)
