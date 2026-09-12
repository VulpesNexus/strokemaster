"""Render saved numeric evidence and create/check a hash-based review freeze.

No geometry is recomputed, no font program is embedded, and no file permissions
or Git state are changed. Run after the design and evidence are ready for review.
"""
import argparse
import hashlib
from html import escape
import json
from pathlib import Path
import re

REPO = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
MANIFEST = HERE / "topology_freeze.json"
BASELINE = "c4596837a5e0ea10a68f7b94e2dd368032341334"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify_evidence():
    data = json.loads((HERE / "topology_evidence.json").read_text(encoding="utf-8"))
    if data["status"] != "PASS" or any(data[k] for k in ("failures", "errors", "skips")):
        raise ValueError("Evidence is not a passing, unskipped run")
    for name, digest in data["source_hashes_sha256"].items():
        if sha(REPO / name) != digest:
            raise ValueError("Evidence/source mismatch: " + name)
    if sha(HERE / "topology_build/topology_region.exe") != data["executable_sha256"]:
        raise ValueError("Evidence/executable mismatch")
    if data.get("core_api_executable_sha256") != sha(HERE / "topology_build/core_api/topology_core_smoke.exe"):
        raise ValueError("Evidence/core-only executable mismatch")
    return data


def render(data):
    from PIL import Image, ImageDraw, ImageFont
    width, height = 1300, 980
    canvas = Image.new("RGB", (width, height), "white")
    pen = ImageDraw.Draw(canvas)
    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
           f'viewBox="0 0 {width} {height}">',
           '<title>Filled-region versus symmetric boundary stroke: synthetic controls</title>',
           f'<rect width="{width}" height="{height}" fill="white"/>']

    def text(x, y, value, size=18, color="#253547"):
        pen.text((x,y), value, font=ImageFont.load_default(size=size), fill=color)
        svg.append(f'<text x="{x}" y="{y+size}" font-family="sans-serif" '
                   f'font-size="{size}" fill="{color}">{escape(value)}</text>')

    text(42,22,"Positive filled-region buffer: two controlled comparisons",28)
    text(42,66,"h = 1.5; miter limit = 4; no external clipping; Boost.Geometry 1.74",19)
    text(42,98,"Gray = original fill. Orange = fill + symmetric boundary stroke. Green = positive filled buffer.",17)
    for row, comparison in enumerate(data["closed_region_comparisons"]):
        result = comparison["result"]
        if result["envelope_source"] != "filled_region_buffer" or result["clipped_area_normalized"] != 0:
            raise ValueError("Graphic requires unclipped positive-buffer measurements")
        source = []
        for wkt in comparison["request"]["source"]:
            if not wkt.startswith("POLYGON("):
                raise ValueError("Graphic supports only saved POLYGON evidence")
            source.append([[tuple(map(float, pair.split())) for pair in ring.split(",")]
                           for ring in re.findall(r"\(([^()]+)\)", wkt)])
        for column, key in enumerate(("symmetric_geometry", "geometry")):
            left, top, pw, ph = 60+620*column, 187+390*row, 560, 252
            local = Image.new("RGB", (pw,ph), "white")
            draw = ImageDraw.Draw(local)
            panel_id = f"panel-{row}-{column}"
            svg.append(f'<defs><clipPath id="{panel_id}"><rect x="{left}" y="{top}" '
                       f'width="{pw}" height="{ph}"/></clipPath></defs>')
            svg.append(f'<g clip-path="url(#{panel_id})">')

            def xy(p):
                return ((p[0]+10)*28,(7-p[1])*28)

            def region(polygons, color):
                for polygon in polygons:
                    mask = Image.new("L", (pw,ph), 0)
                    painter = ImageDraw.Draw(mask)
                    for index, ring in enumerate(polygon):
                        painter.polygon([xy(p) for p in ring], fill=255 if index==0 else 0)
                    local.paste(color, (0,0,pw,ph), mask)
                    commands = []
                    for ring in polygon:
                        points = [xy(p) for p in ring]
                        commands.append("M " + " L ".join(f"{x+left:.6f} {y+top:.6f}" for x,y in points) + " Z")
                    svg.append(f'<path d="{" ".join(commands)}" fill="{color}" fill-rule="evenodd"/>')

            color = "#e4a36b" if column==0 else "#62b7a6"
            region(result[key], color)
            region(source, "#dce2e9")
            for polygon in source:
                for ring in polygon:
                    pts = [xy(p) for p in ring]
                    draw.line(pts, fill="#526479", width=1)
                    svg.append('<path d="M ' + ' L '.join(f'{x+left:.6f} {y+top:.6f}' for x,y in pts)
                               + '" fill="none" stroke="#526479" stroke-width="1"/>')
            ref_y = (7-1.8)*28
            for start in range(0,pw,12):
                draw.line([(start,ref_y),(min(start+7,pw),ref_y)],fill="#34465b",width=1)
            svg.append(f'<path d="M {left} {top+ref_y} H {left+pw}" stroke="#34465b" '
                       'stroke-width="1" stroke-dasharray="7 5"/>')
            if row==0:
                px,py = xy((0,3))
                for ends in (((px-4,py),(px+4,py)),((px,py-4),(px,py+4))):
                    draw.line(ends,fill="#263547",width=2)
                    svg.append(f'<path d="M {ends[0][0]+left} {ends[0][1]+top} '
                               f'L {ends[1][0]+left} {ends[1][1]+top}" stroke="#263547" stroke-width="2"/>')
            svg.append('</g>')
            canvas.paste(local,(left,top))
            pen.rectangle((left,top,left+pw,top+ph),outline="#a9b3be",width=1)
            svg.append(f'<rect x="{left}" y="{top}" width="{pw}" height="{ph}" '
                       'fill="none" stroke="#a9b3be"/>')
            title = ("Triangular hole" if row==0 else "Narrow notch") + (" / symmetric boundary" if column==0 else " / filled region")
            text(left,top-49,title,20)
            max_y = max(y for polygon in result[key] for x,y in polygon[0])
            text(left,top-23,f"Measured top y = {max_y:.6f}",16)
            for tick in (-10,-5,0,5,10):
                text(left+(tick+10)*28-10,top+ph+6,str(tick),13)
            for tick in (0,3,6):
                text(left-23,top+(7-tick)*28-8,str(tick),13)
        caption = ("Hole: the cross at (0, 3) is inside only the symmetric result; ordinary outside corners remain."
                   if row==0 else "Notch control: both results agree; no material excess is claimed.")
        text(60,top+ph+30,caption,17)
    text(42,920,"Dashed y = 1.8 is an analytic reference, not a clipping plane. Upper-region detail; equal axis scale.",16)
    text(42,948,"Synthetic polygon evidence only. No glyphs, embedded font programs, host rendering, or ImageGen.",15)
    svg.append('</svg>')
    (HERE / "topology_oracle.svg").write_text("\n".join(svg)+"\n",encoding="utf-8")
    canvas.save(HERE / "topology_oracle.png")


def snapshot():
    data = verify_evidence()
    render(data)
    names = [*data["source_hashes_sha256"], "TOPOLOGY_GEOMETRY_DESIGN.md",
             "tests/geometry/freeze_topology.py", "tests/geometry/topology_evidence.json",
             "tests/geometry/topology_oracle.svg", "tests/geometry/topology_oracle.png",
             "tests/geometry/topology_build/.gitignore"]
    files = {name:{"sha256":sha(REPO/name),"bytes":(REPO/name).stat().st_size} for name in names}
    digest = hashlib.sha256(json.dumps(files,sort_keys=True,separators=(",",":")).encode()).hexdigest()
    manifest = {"schema":1,"snapshot_sha256":digest,"baseline_commit":BASELINE,
                "review_status":"Prior snapshot rejected for input-union topology loss; pre-union repair awaits independent review; no native admission",
                "supersedes_snapshot_sha256":"6e7d8faf5a519cca33f9869ab90d2dc82904a68b9edbb727f4cfc681ff3a9170",
                "freeze_meaning":"Hash snapshot only; no commit, SDK vendoring, or filesystem locking",
                "files":files,"local_build_artifacts":{
                    name:sha(HERE/"topology_build"/name) for name in ("topology_region.exe","compile.log",
                        "core_api/topology_core_smoke.exe","core_api/compile.log")},
                "limits":{"vertices_per_polygon_list":20000,"polygons_per_list":256,
                          "curve_support":False,"font_programs_included":False,"native_ready_for_production":False}}
    MANIFEST.write_text(json.dumps(manifest,indent=2)+"\n",encoding="utf-8")
    return digest


def check():
    verify_evidence()
    frozen = json.loads(MANIFEST.read_text(encoding="utf-8"))
    for name, item in frozen["files"].items():
        if sha(REPO/name) != item["sha256"] or (REPO/name).stat().st_size != item["bytes"]:
            raise ValueError("Freeze changed: " + name)
    for name,digest in frozen["local_build_artifacts"].items():
        if sha(HERE/"topology_build"/name) != digest:
            raise ValueError("Local build changed: " + name)
    digest = hashlib.sha256(json.dumps(frozen["files"],sort_keys=True,separators=(",",":")).encode()).hexdigest()
    if digest != frozen["snapshot_sha256"]:
        raise ValueError("Snapshot manifest mismatch")
    return digest


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check",action="store_true",help="Verify the freeze without writing")
    args = parser.parse_args()
    print(("Verified " if args.check else "Frozen ") + (check() if args.check else snapshot()))
