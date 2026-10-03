"""Measure the first matching direct-page stroke text run; bounded calibration only."""
import hashlib
import json
from pathlib import Path
import pikepdf
ROOT=Path(__file__).resolve().parents[1]


def first_run(path, target, required_limit=None):
    with pikepdf.open(path) as pdf:
        page=pdf.pages[0]
        if float(page.get('/UserUnit',1)) != 1 or int(page.get('/Rotate',0)) != 0:
            raise ValueError('rotated or rescaled page is outside calibration scope')
        state={'width':1,'miter':10,'join':0,'ctm':[1,0,0,1,0,0],'font':None,'size':None,'mode':0,'tm':None}
        stack=[]
        def compose(a,b):
            x,y,z,w,e,f=a; X,Y,Z,W,E,F=b
            return [x*X+z*Y,y*X+w*Y,x*Z+z*W,y*Z+w*W,x*E+z*F+e,y*E+w*F+f]
        for ins in pikepdf.parse_content_stream(page):
            op,args=str(ins.operator),ins.operands
            if op=='q': stack.append(state.copy())
            elif op=='Q': state=stack.pop()
            elif op=='cm': state['ctm']=compose(state['ctm'],list(map(float,args)))
            elif op=='w': state['width']=float(args[0])
            elif op=='M': state['miter']=float(args[0])
            elif op=='j': state['join']=int(args[0])
            elif op=='Tf': state['font'],state['size']=str(args[0]),float(args[1])
            elif op=='Tr': state['mode']=int(args[0])
            elif op=='Tm': state['tm']=list(map(float,args))
            elif op=='gs':
                ext=page.Resources.get('/ExtGState',{}).get(str(args[0]))
                if ext is None or '/Font' in ext:
                    raise ValueError('unknown state or font override')
                for key,dest in [('/LW','width'),('/LJ','join'),('/ML','miter')]:
                    if key in ext: state[dest]=float(ext[key])
            elif op=='Do': raise ValueError('nested XObject unsupported')
            elif op in ('Tj','TJ'):
                shows=[args[0]] if op=='Tj' else [s for s in args[0] if isinstance(s,pikepdf.String)]
                text=b''.join(bytes(s) for s in shows).decode('ascii')
                if text==target and state['mode']==1 and (required_limit is None or state['miter']==required_limit):
                    if state['ctm'][:4]!=[1,0,0,1]:
                        raise ValueError('nonidentity stroke transform is outside this calibration')
                    t=state['tm']
                    if not t or t[1]!=0 or t[2]!=0 or t[0]!=t[3] or t[0]<=0:
                        raise ValueError('nonisotropic text matrix is outside this calibration')
                    return {'pdf_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
                        'target_text':target,'observed':state,'font_size_pt':state['size']*t[0],
                        'stroke_half_width_pt':state['width']/2,
                        'scope':'first matching stroke text run at requested limit, direct page, identity stroke CTM'}
        raise ValueError('matching stroke text run not found')

if __name__=='__main__':
    rows=[first_run(ROOT/'tests/indesign-stroke/B-miter-spikes/actual/B-miter.pdf','VWAMNvwkyzK', required_limit=4),
        first_run(ROOT/'build/takeover/extreme.pdf','Lorem ipsum', required_limit=4)]
    out=ROOT/'build/miter-calibration-limit4.json'
    if out.exists(): raise ValueError('existing calibration record')
    out.write_text(json.dumps(rows,indent=2)+'\n',encoding='utf-8',newline='\n')
    print(json.dumps(rows,indent=2))
