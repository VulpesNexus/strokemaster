"""Resumable bounded refinement; timeout and missing convergence never count as passes."""
import hashlib
import json
from pathlib import Path
import subprocess
from compare_embedded_region import compare

ROOT=Path(__file__).resolve().parents[1]


def deltas(rows, previous):
    if [r['character'] for r in rows] != [r['character'] for r in previous]:
        raise ValueError('refinement glyph identity mismatch')
    area=max(abs(a['symmetric_excess_area_pt2']-b['symmetric_excess_area_pt2']) for a,b in zip(rows,previous))
    bounds=max(abs(x-y) for a,b in zip(rows,previous)
        for key in ('region_bounds_pt','symmetric_bounds_pt') for x,y in zip(a[key],b[key]))
    return area,bounds


def measure(out, run_compare=compare):
    out.mkdir(exist_ok=True)
    runs=[]; converged=False; previous=None; failure=None
    source=ROOT/'tests/indesign-stroke/B-miter-spikes/actual/B-miter.pdf'
    expected_pdf=hashlib.sha256(source.read_bytes()).hexdigest()
    for i in range(6):
        tolerance=0.00015625/(2**i)
        path=out/f'minion-{i}.json'; failed=out/f'failed-{i}.json'
        if failed.exists():
            failure=json.loads(failed.read_text(encoding='utf-8'))
            break
        if path.exists():
            run=json.loads(path.read_text(encoding='utf-8'))
            if run['pdf_sha256']!=expected_pdf or run['tolerance_pt']!=tolerance or run['size_pt']!=36 or run['half_width_pt']!=4 or run['limit']!=4:
                raise ValueError('checkpoint calibration or input mismatch')
        else:
            try:
                run=run_compare(source,'Mvwz',36,4,tolerance,4)
            except subprocess.TimeoutExpired as exc:
                failure={'stage':'core comparison','type':'TimeoutExpired','timeout_seconds':exc.timeout,
                    'tolerance_pt':tolerance,'index':i,'nativeExecuted':False}
                failed.write_text(json.dumps(failure,indent=2)+'\n',encoding='utf-8',newline='\n')
                break
            path.write_text(json.dumps(run,indent=2)+'\n',encoding='utf-8',newline='\n')
        rows=run['fonts'][0]['glyphs']
        if not all(r['status']=='CORE_OK' for r in rows):
            runs.append({'tolerance_pt':tolerance,'outcomes':run['fonts'][0]['outcomes'],'converged':False})
            break
        area=bounds=None
        if previous:
            area,bounds=deltas(rows,previous)
            converged=area<=0.01 and bounds<=0.005
        runs.append({'tolerance_pt':tolerance,'max_area_delta_pt2':area,'max_bounds_delta_pt':bounds,
            'converged':converged,'glyphs':rows})
        previous=rows
        if converged: break
    report={'schema':1,'status':'REFINEMENT_CONVERGED_ON_CORPUS' if converged else 'REFINEMENT_NOT_CONVERGED',
        'nativeExecuted':False,'criterion':{'successive_excess_area_delta_pt2':0.01,'successive_bounds_delta_pt':0.005},
        'converged_on_corpus':converged,'runs':runs,'failure':failure,
        'source_sha256':hashlib.sha256((ROOT/'tools/compare_embedded_region.py').read_bytes()).hexdigest(),
        'limitations':['Successive stability is measured on four glyphs; it is not a universal stroke error bound.',
            'No native control, coordinate-level outline provenance, or Adobe miter acceptance is established.']}
    (out/'summary.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8',newline='\n')
    return report

if __name__=='__main__':
    report=measure(ROOT/'build/miter-refinement')
    print(json.dumps({'status':report['status'],'completed_runs':len(report['runs']),'failure':report['failure']}))
    raise SystemExit(0 if report['converged_on_corpus'] else 2)
