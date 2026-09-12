"""Research PNG: a local clipped inner V is not a global envelope constraint.

Draws only a specified synthetic corner and a surrounding rectangular contour.
No font processing, host rendering, or global clipping algorithm is performed.
"""
import json
import math
from pathlib import Path
import sys

from PIL import Image, ImageDraw, ImageFont
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from miter_rule import join_boundary


def cases():
    h, angle = 1.5, 30.0
    s, c = math.sin(math.radians(angle)/2), math.cos(math.radians(angle)/2)
    clipped = join_boundary((0, 0), (s, c), (s, -c), h, 'clip')
    native = join_boundary((0, 0), (s, c), (s, -c), h, 'miter', limit=4)
    data = []
    for gap_ratio in (0.6, 0.2):
        d = h*gap_ratio
        envelope = d+h
        clip_y = max(p[1] for p in clipped)
        data.append({'half_width': h, 'interior_angle_degrees': angle,
                     'gap_ratio': gap_ratio, 'gap': d, 'outer_envelope_y': envelope,
                     'local_clip_y': clip_y, 'clip_boundary': clipped,
                     'conventional_miter_boundary': native,
                     'exceeds_outer_envelope': clip_y > envelope,
                     'signed_axial_excess': clip_y-envelope,
                     'outer_rectangle': [-5*h, -8*h, 5*h, d],
                     'inner_triangle': [(0, 0), (-5*h*s/c, -5*h), (5*h*s/c, -5*h)]})
    return data


def render(destination):
    supersample = 2
    im = Image.new('RGB', (1600*supersample, 1060*supersample), '#f7f9fc')
    draw = ImageDraw.Draw(im)
    def font(size, bold=False):
        path = Path('C:/Windows/Fonts')/('segoeuib.ttf' if bold else 'segoeui.ttf')
        return ImageFont.truetype(str(path), size*supersample)
    def text(x, y, label, size=19, fill='#263449', bold=False):
        draw.text((x*supersample, y*supersample), label, font=font(size, bold), fill=fill)
    def line(points, fill, width=2):
        draw.line([(x*supersample,y*supersample) for x,y in points], fill=fill, width=width*supersample)
    def polygon(points, fill):
        draw.polygon([(x*supersample,y*supersample) for x,y in points], fill=fill)
    def dashed(a, b, fill='#8a95a6', width=2):
        length = math.hypot(b[0]-a[0], b[1]-a[1])
        n = max(1, int(length/8))
        for k in range(0, n, 2):
            line([(a[0]+(b[0]-a[0])*k/n, a[1]+(b[1]-a[1])*k/n),
                  (a[0]+(b[0]-a[0])*min(k+1,n)/n, a[1]+(b[1]-a[1])*min(k+1,n)/n)], fill, width)
    text(64, 35, 'A local flat clip cannot guarantee a global boundary', 36, bold=True)
    text(64, 91, 'Synthetic inner V: 30 degrees, half-width h = 1.5 pt, local axial cap = sqrt(2)h = 2.121320 pt', 21)
    text(64, 125, 'Only the distance to the surrounding outer contour changes. Both V contours fit strictly inside the parent.', 20)
    results = cases()
    for index, row in enumerate(results):
        left = 64+index*780
        text(left, 190, 'A  /  More clearance' if index == 0 else 'B  /  Less clearance', 27, bold=True)
        text(left, 230, 'd = %.1fh = %.1f pt' % (row['gap_ratio'],row['gap']), 21)
        px, py, scale = left+328, 725, 106
        def xy(p):
            return (px+p[0]*scale, py-p[1]*scale)
        # The main view crops the large parent rectangle and the bottom of the hole.
        d = row['gap_ratio']; outer = d+1
        polygon([xy((-2.8,-1.2)),xy((2.8,-1.2)),xy((2.8,d)),xy((-2.8,d))], '#e2e7ef')
        polygon([xy((-2.8,d)),xy((2.8,d)),xy((2.8,outer)),xy((-2.8,outer))], '#dceee7')
        t = math.tan(math.pi/12)
        polygon([xy((0,0)),xy((-1.2*t,-1.2)),xy((1.2*t,-1.2))], '#ffffff')
        for y in (0,1,2,3,4):
            a,b=xy((-2.8,y)),xy((2.8,y))
            dashed(a,b,'#d6dce5',1)
            text(left+6,a[1]-12,'%dh'%y,16,fill='#66758a')
        # Conventional miter is an analytic reference, not an Adobe rendering.
        original = [(x/row['half_width'],y/row['half_width']) for x,y in row['conventional_miter_boundary']]
        for a,b in zip(original,original[1:]):
            dashed(xy(a),xy(b),'#7e8999',2)
        text(left+365, 293, 'Limit-4 miter tip', 17,fill='#66758a')
        clipped = [(x/row['half_width'],y/row['half_width']) for x,y in row['clip_boundary']]
        polygon([xy((0,0))]+[xy(p) for p in clipped], '#b5d5fb')
        line([xy(p) for p in clipped], '#276dc2', 3)
        if row['exceeds_outer_envelope']:
            s,c=math.sin(math.pi/12),math.cos(math.pi/12)
            x_at_envelope=(1-s*outer)/c
            polygon([xy((-x_at_envelope,outer)),xy(clipped[1]),xy(clipped[2]),xy((x_at_envelope,outer))], '#e797a2')
            line([xy(clipped[1]),xy(clipped[2])], '#b72d49', 4)
        line([xy((-2.8,d)),xy((2.8,d))], '#626f80', 2)
        line([xy((-2.8,outer)),xy((2.8,outer))], '#268363', 3)
        text(left+38,xy((0,outer))[1]-29,'Outer stroke envelope: %.1fh'%outer,18,fill='#17664a',bold=True)
        cap=math.sqrt(2)
        x1,y1=xy((1.0,cap))
        line([xy((0.72,cap)),(x1+22,y1-27)], '#276dc2', 2)
        text(x1+26,y1-43,'Local clip',18,fill='#276dc2',bold=True)
        text(left+385,xy((0,d))[1]+8,'Outer contour, y = d',17,fill='#505e70')
        line([xy((-1.2*t,-1.2)),xy((0,0)),xy((1.2*t,-1.2))], '#334155', 3)
        text(left+174,810,'Inner hole (V apex at y = 0)',18)
        if index == 0:
            text(left,873,'Contained in this example',25,fill='#17664a',bold=True)
            text(left,913,'Clearance above clip: %.6f pt'%(-row['signed_axial_excess']),21)
        else:
            text(left,873,'Crosses the outer envelope',25,fill='#a7213e',bold=True)
            text(left,913,'Red excess: %.6f pt'%row['signed_axial_excess'],21)
    text(64,973,'For this flat boundary: containment requires d + h >= sqrt(2)h, or d >= (sqrt(2) - 1)h.',22,bold=True)
    text(64,1011,'Research geometry only. No font pass, host result, whole-stroke union, or global repair is claimed.',19,fill='#5c6b80')
    im.resize((1600,1060), Image.Resampling.LANCZOS).save(destination)
    return results


if __name__ == '__main__':
    directory=Path(__file__).resolve().parent
    result=render(directory/'local_vs_global_envelope.png')
    (directory/'local_vs_global_envelope.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps([{k:v for k,v in row.items() if k not in ('clip_boundary','conventional_miter_boundary','outer_rectangle','inner_triangle')} for row in result],indent=2))
