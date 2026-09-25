"""Plan and measure the route W certification runs.

CLI:
  plan SESSION [--families ...]     write build/cert/SESSION/spec.json for tools/jsx/cert-run.jsx
  measure SESSION OUTPUT --gs GSWIN64C --work ASCII_DIR [--label TEXT]

A session is one InDesign process with one plugin state (route W installed, or the refusal
build as the control). Every source is copied before it is opened (tools/jsx/cert-lib.jsx).
For each source and output family the plan writes three documents' worth of output:

- out:  the source as it is, the output under test;
- fill: the source with every text stroke set to [None]. Removing the strokes changes nothing a
        correct fill should show, so inside the fill core the output must equal this twin;
- mask: fills in process white over black, from which the fill core is eroded.

Fill-core damage is the count of core pixels more than 64 levels from the fill twin rendered
by the same renderer at the same resolution (InDesign for PNG, MuPDF for PDF, Ghostscript for
EPS and PostScript). The victim census also gets its split reference (the census oracle), for
the edge measurements that separate semantic damage from rasterization variance.

User-defined PDF presets are named only in build/cert/user-presets.json (ignored); the plan and
the record refer to them as U1, U2, and so on, with their compatibility and color settings.
"""
import argparse
import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
CORPUS = REPO / 'tests/certification/corpus'
CERT = REPO / 'build/cert'

SOURCES = {'V': 'victims', 'T': 'cert-T', 'C': 'cert-C', 'E': 'cert-E'}

# id: (preset, overrides, preparation). Overrides use enum:<Enumeration>:<KEY> for enumerations.
PDF_FAMILIES = {
    'hqp': ('[High Quality Print]', {}, None),
    'press': ('[Press Quality]', {}, None),
    'small': ('[Smallest File Size]', {}, None),
    'x1a': ('[PDF/X-1a:2001]', {}, None),
    'x3': ('[PDF/X-3:2002]', {}, None),
    'x4': ('[PDF/X-4:2008]', {}, None),
    'a4': ('[High Quality Print]', {'acrobatCompatibility': 'enum:AcrobatCompatibility:ACROBAT_4'}, None),
    'a4low': ('[High Quality Print]', {'acrobatCompatibility': 'enum:AcrobatCompatibility:ACROBAT_4'}, 'flatten:LOW:no:yes'),
    'a4outline': ('[High Quality Print]', {'acrobatCompatibility': 'enum:AcrobatCompatibility:ACROBAT_4'}, 'flatten:HIGH:yes:no'),
    'a8': ('[High Quality Print]', {'acrobatCompatibility': 'enum:AcrobatCompatibility:ACROBAT_8'}, None),
    'gray': ('[High Quality Print]', {'pdfColorSpace': 'enum:PDFColorSpace:GRAY'}, None),
    'rgb': ('[High Quality Print]', {'pdfColorSpace': 'enum:PDFColorSpace:REPURPOSE_RGB'}, None),
    'editable': ('[High Quality Print]', {'preserveInDesignEditingCapabilities': True}, None),
}
PNG_SWEEP = [
    ('png72', {'ppi': 72}), ('png150', {'ppi': 150}), ('png300', {'ppi': 300}), ('png600', {'ppi': 600}),
    ('png300noaa', {'ppi': 300, 'antiAlias': False}), ('png600noaa', {'ppi': 600, 'antiAlias': False}),
    ('png600transparent', {'ppi': 600, 'transparentBackground': True}), ('png600gray', {'ppi': 600, 'gray': True}),
]
PRINT = {
    'eps': ('eps', {}), 'eps3rgb': ('eps', {'color': 'RGB', 'level': 'LEVEL_3'}),
    'pshigh': ('ps', {'flattener': '[High Resolution]'}), 'pslow': ('ps', {'flattener': '[Low Resolution]'}),
    'psgray': ('ps', {'flattener': '[High Resolution]', 'colorOutput': 'COMPOSITE_GRAY'}),
}
# Which families each source gets. The victim census gets everything; the others get the
# families that exercise a different Adobe code path for what they vary.
PLAN = {
    'V': {'pdf': list(PDF_FAMILIES) + ['U1', 'U2'], 'png': [p for p, _ in PNG_SWEEP], 'print': list(PRINT)},
    'T': {'pdf': ['hqp', 'x1a', 'x3', 'x4', 'a4', 'a4low', 'a4outline', 'press', 'U1'], 'png': ['png600'], 'print': ['eps', 'pshigh', 'pslow']},
    'C': {'pdf': ['hqp', 'press', 'x1a', 'x4', 'gray', 'rgb', 'U1', 'U2'], 'png': ['png600', 'png600gray'], 'print': ['eps', 'pshigh', 'psgray']},
    'E': {'pdf': ['hqp', 'x1a', 'x4'], 'png': ['png600'], 'print': ['eps', 'pshigh']},
}


# The A/B sessions (the refusal build as control, then route W again in a fresh process) repeat
# a subset: every architecture-distinct family once, not every preset.
PLAN_CONTROL = {
    'V': {'pdf': ['hqp', 'x4', 'x1a', 'a4low'], 'png': ['png300', 'png600'], 'print': ['eps', 'pshigh']},
    'T': {'pdf': ['hqp', 'x4', 'x1a', 'a4', 'a4low'], 'png': ['png600'], 'print': ['eps', 'pshigh']},
    'C': {'pdf': ['hqp', 'press', 'x1a'], 'png': ['png600'], 'print': ['pshigh']},
    'E': {'pdf': ['hqp', 'x1a'], 'png': ['png600'], 'print': ['pshigh']},
}
SUITES = ['A-adjacent-line-overlap/minimal/A-minimal.indd', 'B-miter-spikes/minimal/B-miter.indd',
          'C-run-fragmentation/minimal/C-attribute-sweep.indd', 'N-transparency/minimal/N-transparency.indd',
          'S-split/minimal/S-split.indd']


def fwd(path):
    return str(path).replace('\\', '/')


def user_presets():
    path = CERT / 'user-presets.json'
    return json.loads(path.read_text(encoding='utf-8')) if path.exists() else {}


def pdf_outputs(families, users):
    outs, preps = [], {}
    for fam in families:
        if fam in PDF_FAMILIES:
            preset, overrides, prep = PDF_FAMILIES[fam]
        elif fam in users:
            preset, overrides, prep = users[fam], {}, None
        else:
            continue
        preps.setdefault(prep, []).append({'kind': 'pdf', 'preset': preset, 'overrides': overrides, 'file': fam + '.pdf'})
    return preps


def source_jobs(session, key, families, users):
    out_root = CERT / session / 'out'
    work = CERT / session / 'work'
    src = CORPUS / (SOURCES[key] + '.indd')
    jobs = []
    png = dict(PNG_SWEEP)
    by_prep = pdf_outputs(families['pdf'], users)
    plain = by_prep.pop(None, [])
    plain += [{'kind': 'png', **png[p], 'file': p + '.png'} for p in families['png']]
    plain += [{'kind': PRINT[p][0], **PRINT[p][1], 'file': p + ('.eps' if PRINT[p][0] == 'eps' else '.ps')} for p in families['print']]
    twins = [('out', []), ('fill', ['fillOnly'])] + ([('ref', ['splitReference'])] if key == 'V' else [])
    for twin, prep in twins:
        outputs = plain if twin != 'ref' else [o for o in plain if o['kind'] != 'ps' or o['file'] == 'pshigh.ps']
        jobs.append({'id': '%s.%s' % (key, twin), 'source': fwd(src), 'copy': fwd(work / ('%s.%s.indd' % (key, twin))),
                     'prep': prep, 'outDir': fwd(out_root / key / twin), 'outputs': outputs})
        for extra_prep, outs in by_prep.items():
            jobs.append({'id': '%s.%s.%s' % (key, twin, extra_prep.split(':')[1].lower() + extra_prep.split(':')[2]),
                         'source': fwd(src), 'copy': fwd(work / ('%s.%s.%s.indd' % (key, twin, outs[0]['file'][:-4]))),
                         'prep': prep + [extra_prep], 'outDir': fwd(out_root / key / twin), 'outputs': outs})
    ppis = sorted({png[p]['ppi'] for p in families['png']})
    jobs.append({'id': '%s.mask' % key, 'source': fwd(src), 'copy': fwd(work / ('%s.mask.indd' % key)), 'prep': ['mask'],
                 'outDir': fwd(out_root / key / 'mask'),
                 'outputs': [{'kind': 'png', 'ppi': p, 'gray': True, 'file': 'mask%d.png' % p} for p in ppis]})
    return jobs


def roundtrip_jobs(session):
    """INDD save and reopen, IDML, package, and placing the exports back into InDesign."""
    out_root, work = CERT / session / 'out', CERT / session / 'work'
    v = CORPUS / 'victims.indd'
    rt = out_root / 'RT'
    std = [{'kind': 'pdf', 'preset': '[High Quality Print]', 'overrides': {}, 'file': 'hqp.pdf'},
           {'kind': 'png', 'ppi': 600, 'file': 'png600.png'}]
    jobs = [
        {'id': 'RT.save', 'source': fwd(v), 'copy': fwd(work / 'RT.save.src.indd'), 'prep': [], 'outDir': fwd(rt / 'save'),
         'outputs': [{'kind': 'indd', 'file': 'saved.indd'}, {'kind': 'idml', 'file': 'victims.idml'}, {'kind': 'pkg', 'file': 'package'}]},
        {'id': 'RT.reopen', 'source': fwd(rt / 'save/saved.indd'), 'copy': fwd(work / 'RT.reopen.indd'), 'prep': [],
         'outDir': fwd(rt / 'reopen'), 'outputs': std},
        {'id': 'RT.idml', 'source': fwd(rt / 'save/victims.idml'), 'copy': fwd(work / 'RT.idml.idml'), 'prep': [],
         'outDir': fwd(rt / 'idml'), 'outputs': std + [{'kind': 'indd', 'file': 'from-idml.indd'}]},
        {'id': 'RT.idml-reopen', 'source': fwd(rt / 'idml/from-idml.indd'), 'copy': fwd(work / 'RT.idml-reopen.indd'), 'prep': [],
         'outDir': fwd(rt / 'idml-reopen'), 'outputs': std},
        {'id': 'RT.package', 'source': fwd(rt / 'save/package/RT.save.src.indd'), 'copy': fwd(rt / 'save/package/RT.package-copy.indd'),
         'prep': [], 'outDir': fwd(rt / 'package'), 'outputs': std},
    ]
    for stem, fam in (('hqp', 'hqp.pdf'), ('x4', 'x4.pdf'), ('x1a', 'x1a.pdf'), ('eps', 'eps.eps')):
        for twin in ('out', 'fill'):
            jobs.append({'id': 'RT.place-%s.%s' % (stem, twin), 'place': {'file': fwd(out_root / 'V' / twin / fam), 'size': [800, 1000]},
                         'outDir': fwd(rt / ('place-' + stem) / twin), 'outputs': std})
    return jobs


def suite_jobs(session):
    """The saved fixture suites, re-exported as they are: route W against the native control."""
    out_root, work = CERT / session / 'out' / 'SUITE', CERT / session / 'work'
    jobs = []
    for rel in SUITES:
        stem = Path(rel).stem
        jobs.append({'id': 'SUITE.' + stem, 'source': fwd(REPO / 'tests/indesign-stroke' / rel), 'copy': fwd(work / ('SUITE.%s.indd' % stem)),
                     'prep': [], 'outDir': fwd(out_root / stem),
                     'outputs': [{'kind': 'pdf', 'preset': '[High Quality Print]', 'overrides': {}, 'file': 'hqp.pdf'},
                                 {'kind': 'png', 'ppi': 300, 'file': 'png300.png'}]})
    return jobs


def saved_jobs(session, origin='A1'):
    """Documents saved in the route W session (plain save, IDML conversion, edited), opened here."""
    src = CERT / origin / 'out'
    out_root, work = CERT / session / 'out' / 'SAVED', CERT / session / 'work'
    std = [{'kind': 'pdf', 'preset': '[High Quality Print]', 'overrides': {}, 'file': 'hqp.pdf'},
           {'kind': 'png', 'ppi': 600, 'file': 'png600.png'}]
    docs = {'saved': src / 'RT/save/saved.indd', 'from-idml': src / 'RT/idml/from-idml.indd',
            'ED1-edited': src / 'ED/ED1-edited.indd', 'ED2-edited': src / 'ED/ED2-edited.indd'}
    return [{'id': 'SAVED.' + k, 'source': fwd(v), 'copy': fwd(work / ('SAVED.%s.indd' % k)), 'prep': [],
             'outDir': fwd(out_root / k), 'outputs': std} for k, v in docs.items()]


def edit_jobs(session):
    """Fill twins and masks for the edited states tools/jsx/cert-edit.jsx saved."""
    ed = CERT / session / 'out' / 'ED'
    work = CERT / session / 'work'
    std = [{'kind': 'pdf', 'preset': '[High Quality Print]', 'overrides': {}, 'file': 'hqp.pdf'},
           {'kind': 'png', 'ppi': 600, 'file': 'png600.png'}]
    jobs = []
    for sid in ('ED1', 'ED2'):
        src = fwd(ed / (sid + '-edited.indd'))
        jobs.append({'id': sid + '.fill', 'source': src, 'copy': fwd(work / (sid + '.fill.indd')), 'prep': ['fillOnly'],
                     'outDir': fwd(ed / (sid + '-twin')), 'outputs': std})
        jobs.append({'id': sid + '.mask', 'source': src, 'copy': fwd(work / (sid + '.mask.indd')), 'prep': ['mask'],
                     'outDir': fwd(ed / (sid + '-twin')), 'outputs': [{'kind': 'png', 'ppi': 600, 'gray': True, 'file': 'mask600.png'}]})
    return jobs


def plan(session, families, control=False, spec_name='spec.json'):
    users = user_presets()
    jobs = []
    for key in families:
        if key == 'RT':
            jobs += roundtrip_jobs(session)
        elif key == 'ED':
            jobs += edit_jobs(session)
        elif key == 'SUITE':
            jobs += suite_jobs(session)
        elif key == 'SAVED':
            jobs += saved_jobs(session)
        elif key == 'REDRAW':
            jobs += redraw_jobs(session)
        else:
            jobs += source_jobs(session, key, (PLAN_CONTROL if control else PLAN)[key], users)
    spec = {'session': session, 'results': fwd(CERT / session / 'results.jsonl'), 'jobs': jobs}
    path = CERT / session / spec_name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(spec, indent=1), encoding='utf-8', newline='\n')
    print('%s: %d jobs, %d outputs -> %s' % (session, len(jobs), sum(len(j['outputs']) for j in jobs), path))


# ---- measurement

def manifest_frames(key):
    path = CORPUS / (SOURCES[key] + '.manifest.json')
    return json.loads(path.read_text(encoding='utf-8'))['frames']


def file_sha(path):
    import hashlib
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


class Renders:
    """Render any output to grayscale at a given resolution with its own format's renderer."""

    def __init__(self, gs, work):
        self.gs, self.work = gs, Path(work)
        self.n = 0

    def __call__(self, path, ppi):
        import numpy
        import pymupdf
        import record_routew_print as printing
        import routew_measure as m
        path = Path(path)
        ext = path.suffix.lower()
        if ext == '.png':
            return m.gray(path), 'InDesign PNG'
        if ext == '.pdf':
            doc = pymupdf.open(str(path))
            pix = doc[0].get_pixmap(dpi=ppi, colorspace=pymupdf.csGRAY, alpha=False)
            return numpy.frombuffer(pix.samples, numpy.uint8).reshape(pix.height, pix.width), 'MuPDF ' + pymupdf.VersionBind
        if ext in ('.eps', '.ps'):
            self.n += 1
            png = printing.render(self.gs, self.work, path, 'r%d' % self.n, ppi, ext == '.eps')
            arr = m.gray(png)
            png.unlink()
            (self.work / ('r%d.ps' % self.n)).unlink()
            return arr, 'Ghostscript'
        raise ValueError(path)


def output_ppi(name):
    png = dict(PNG_SWEEP)
    stem = Path(name).stem
    return png[stem]['ppi'] if stem in png else 600


def backdrop_level(ref, ppi):
    # The census backdrop (Black 50%) at a point 5 pt inside the page corner.
    at = max(1, int(5 * ppi / 72))
    return int(ref[at, at])


def measure_source(key, session_dir, render, masks_cache):
    import routew_measure as m
    frames = manifest_frames(key)
    base = session_dir / 'out' / key
    record = {'frames': [{k: f.get(k) for k in ('key', 'name', 'bounds', 'buildError')} for f in frames], 'outputs': {}}
    for out in sorted((base / 'out').iterdir()):
        twin = base / 'fill' / out.name
        ref = base / 'ref' / out.name
        ppi = output_ppi(out.name)
        if (key, ppi) not in masks_cache:
            masks_cache[(key, ppi)] = m.core_of(m.gray(base / 'mask' / ('mask%d.png' % ppi)), ppi)
        core = masks_cache[(key, ppi)]
        o, renderer = render(out, ppi)
        t, _ = render(twin, ppi)
        row = {'sha256': file_sha(out), 'bytes': out.stat().st_size, 'renderer': renderer,
               'fill_core': m.core_damage(o, t, core, frames, ppi)}
        if out.suffix.lower() == '.pdf':
            row['structure'] = m.pdf_structure(str(out), frames)
            row['twin_text_chars'] = m.pdf_structure(str(twin), frames)['text_chars']
        if ref.exists():
            r, _ = render(ref, ppi)
            row['edge_vs_split_reference'] = m.edge_metrics(o, r, frames, ppi, backdrop=backdrop_level(r, ppi))
            if out.suffix.lower() == '.pdf':
                rs = m.pdf_structure(str(ref), frames)
                row['reference_structure'] = {kk: rs[kk] for kk in ('text_chars', 'text_render_modes', 'images')}
        record['outputs'][out.name] = row
        print(key, out.name, 'damage', row['fill_core']['damage_px_total'], flush=True)
    return record


def measure_roundtrip(session_dir, render, masks_cache):
    import routew_measure as m
    frames = manifest_frames('V')
    rt = session_dir / 'out' / 'RT'
    vbase = session_dir / 'out' / 'V'
    rec = {}
    for sub in sorted(p for p in rt.iterdir() if p.is_dir() and p.name != 'save'):
        if (sub / 'out').exists():
            pairs = [(sub / 'out' / f.name, sub / 'fill' / f.name) for f in sorted((sub / 'out').iterdir())]
        else:
            pairs = [(f, vbase / 'fill' / f.name) for f in sorted(sub.iterdir()) if f.suffix.lower() in ('.pdf', '.png')]
        rows = {}
        for out, twin in pairs:
            ppi = output_ppi(out.name)
            if ('V', ppi) not in masks_cache:
                masks_cache[('V', ppi)] = m.core_of(m.gray(vbase / 'mask' / ('mask%d.png' % ppi)), ppi)
            o, renderer = render(out, ppi)
            t, _ = render(twin, ppi)
            same, _ = render(vbase / 'out' / out.name, ppi)
            (a, b), trimmed = m.align(o, same)
            d = abs(a.astype(int) - b.astype(int))
            row = {'sha256': file_sha(out), 'renderer': renderer, 'twin': str(twin.relative_to(session_dir)).replace('\\', '/'),
                   'fill_core': m.core_damage(o, t, masks_cache[('V', ppi)], frames, ppi),
                   'px_differing_from_direct_export': int((d > 0).sum()), 'px_over_64_from_direct_export': int((d > 64).sum()),
                   'size_trimmed_px': trimmed}
            if out.suffix.lower() == '.pdf':
                row['structure'] = m.pdf_structure(str(out), frames)
            rows[out.name] = row
            print('RT', sub.name, out.name, 'damage', row['fill_core']['damage_px_total'], 'off direct', row['px_over_64_from_direct_export'], flush=True)
        rec[sub.name] = rows
    return rec


def measure_illustrator(session_dir, render, masks_cache):
    """Route W PDFs opened and re-saved by Illustrator (tools/jsx/ai-cert-roundtrip.jsx). Each saved
    file is compared with its fill twin taken through the same Illustrator step, and with the
    InDesign export it came from."""
    import routew_measure as m
    frames = manifest_frames('V')
    ai = session_dir / 'out' / 'AI'
    vbase = session_dir / 'out' / 'V'
    if ('V', 600) not in masks_cache:
        masks_cache[('V', 600)] = m.core_of(m.gray(vbase / 'mask' / 'mask600.png'), 600)
    inventories, rows = [], {}
    for run in sorted(p for p in ai.iterdir() if p.is_dir()):
        inventories += json.loads((run / 'ai-results.json').read_text(encoding='utf-8'))
        for out in sorted(run.glob('*-out-*.pdf')):
            fam, _, step = out.stem.partition('-out-')
            twin = run / ('%s-fill-%s.pdf' % (fam, step))
            o, renderer = render(out, 600)
            t, _ = render(twin, 600)
            src, _ = render(vbase / 'out' / (fam + '.pdf'), 600)
            (a, b), trimmed = m.align(o, src)
            d = abs(a.astype(int) - b.astype(int))
            rows[out.name] = {'sha256': file_sha(out), 'bytes': out.stat().st_size, 'renderer': renderer,
                              'fill_core': m.core_damage(o, t, masks_cache[('V', 600)], frames, 600),
                              'px_over_64_from_indesign_export': int((d > 64).sum()), 'px_differing_from_indesign_export': int((d > 0).sum()),
                              'structure': m.pdf_structure(str(out), frames)}
            print('AI', out.name, 'damage', rows[out.name]['fill_core']['damage_px_total'], 'off InDesign', rows[out.name]['px_over_64_from_indesign_export'], flush=True)
    return {'inventories': inventories, 'outputs': rows}


def _pair_stats(render, a_path, b_path, ppi):
    import routew_measure as m
    a, _ = render(a_path, ppi)
    b, _ = render(b_path, ppi)
    (a, b), trimmed = m.align(a, b)
    d = abs(a.astype(int) - b.astype(int))
    return {'px_differing': int((d > 0).sum()), 'px_over_64': int((d > 64).sum()), 'max_diff': int(d.max()), 'trimmed_px': trimmed}


def _core(masks_cache, key, path, ppi=600):
    import routew_measure as m
    if key not in masks_cache:
        masks_cache[key] = m.core_of(m.gray(path), ppi)
    return masks_cache[key]


def measure_editing(session_dir, render, masks_cache):
    """tools/jsx/cert-edit.jsx: each stage against its fill twin, and the identities an edit
    history must keep (undo returns to the original drawing, redo and reopen to the edited one)."""
    import routew_measure as m
    ed = session_dir / 'out' / 'ED'
    base = session_dir / 'out'
    stages = [json.loads(line) for line in (ed / 'results.jsonl').read_text(encoding='utf-8').splitlines()]
    frames = {'ED1': manifest_frames('V'), 'ED2': manifest_frames('E') + [{'key': 'E01dup', 'bounds': [605, 20, 665, 190]}], 'ED3': manifest_frames('V')}
    original_twin = {'ED1': ('V', base / 'V'), 'ED2': ('E', base / 'E'), 'ED3': ('V', base / 'V')}
    rec = {'stages': {}, 'identities': {}}
    for st in stages:
        sid, _, name = st['id'].partition('-')
        edited = name in ('live', 'redone', 'reopened')
        rows = {}
        for f in ('hqp.pdf', 'png600.png'):
            out = ed / st['id'] / f
            if edited:
                twin, core = ed / (sid + '-twin') / f, _core(masks_cache, (sid, 'edited'), ed / (sid + '-twin') / 'mask600.png')
            else:
                key, root = original_twin[sid]
                twin, core = root / 'fill' / f, _core(masks_cache, (key, 600), root / 'mask' / 'mask600.png')
            o, renderer = render(out, 600)
            t, _ = render(twin, 600)
            rows[f] = {'sha256': file_sha(out), 'renderer': renderer, 'fill_core': m.core_damage(o, t, core, frames[sid], 600)}
            print('ED', st['id'], f, 'damage', rows[f]['fill_core']['damage_px_total'], flush=True)
        rec['stages'][st['id']] = {'undo_history': st.get('undo'), 'undo_steps': st.get('undoSteps'), 'redo_steps': st.get('redoSteps'),
                                   'log': st.get('log'), 'errors': [o['error'] for o in st['outputs'] if o['error']], 'outputs': rows}
    checks = [('ED1-undone', 'ED1-original'), ('ED1-redone', 'ED1-live'), ('ED1-reopened', 'ED1-live'),
              ('ED2-undone', 'ED2-original'), ('ED2-redone', 'ED2-live'), ('ED2-reopened', 'ED2-live'),
              ('ED3-cycle2', 'ED1-original'), ('ED3-cycle4', 'ED1-original'), ('ED3-cycle3', 'ED3-cycle1'), ('ED3-cycle5', 'ED3-cycle1')]
    for a, b in checks:
        for f in ('hqp.pdf', 'png600.png'):
            rec['identities']['%s vs %s %s' % (a, b, f)] = _pair_stats(render, ed / a / f, ed / b / f, 600)
    for f in ('hqp.pdf', 'png600.png'):
        rec['identities']['ED1-original vs V direct export %s' % f] = _pair_stats(render, ed / 'ED1-original' / f, base / 'V' / 'out' / f, 600)
    return rec


def measure_suites(session_dir, render):
    """Hashes and structure of the fixture-suite exports; compare-sessions pairs them across sessions."""
    import routew_measure as m
    rec = {}
    for rel in SUITES:
        stem = Path(rel).stem
        folder = session_dir / 'out' / 'SUITE' / stem
        manifest = REPO / 'tests/indesign-stroke' / Path(rel).parts[0] / 'actual' / (stem + '.manifest.json')
        frames = json.loads(manifest.read_text(encoding='utf-8'))['frames'] if manifest.exists() else []
        for f in frames:
            f.setdefault('key', f.get('name'))
        rec[stem] = {'manifest': str(manifest.relative_to(REPO)).replace('\\', '/') if manifest.exists() else None,
                     'pdf_sha256': file_sha(folder / 'hqp.pdf'), 'png_sha256': file_sha(folder / 'png300.png'),
                     'structure': {k: v for k, v in m.pdf_structure(str(folder / 'hqp.pdf'), frames).items() if k != 'frames'}}
    return rec


def measure_saved(session_dir, render, masks_cache, origin='A1'):
    """Documents saved in the route W session, opened and exported in this one."""
    import routew_measure as m
    o_out = CERT / origin / 'out'
    base = session_dir / 'out'
    pairs = {'saved': (o_out / 'RT/reopen', base / 'V', 'V'), 'from-idml': (o_out / 'RT/idml-reopen', base / 'V', 'V'),
             'ED1-edited': (o_out / 'ED/ED1-reopened', o_out / 'ED/ED1-twin', 'ED1'), 'ED2-edited': (o_out / 'ED/ED2-reopened', o_out / 'ED/ED2-twin', 'ED2')}
    rec = {}
    for name, (same, twin_root, fkey) in pairs.items():
        rows = {}
        frames = manifest_frames('E') + [{'key': 'E01dup', 'bounds': [605, 20, 665, 190]}] if fkey == 'ED2' else manifest_frames('V')
        for f in ('hqp.pdf', 'png600.png'):
            out = base / 'SAVED' / name / f
            if fkey == 'V':
                twin, core = twin_root / 'fill' / f, _core(masks_cache, ('V', 600), twin_root / 'mask' / 'mask600.png')
            else:
                twin, core = twin_root / f, _core(masks_cache, (fkey, 'edited-origin'), twin_root / 'mask600.png')
            o, renderer = render(out, 600)
            t, _ = render(twin, 600)
            rows[f] = {'sha256': file_sha(out), 'fill_core': m.core_damage(o, t, core, frames, 600),
                       'vs_route_w_session_export': _pair_stats(render, out, same / f, 600)}
            print('SAVED', name, f, 'damage', rows[f]['fill_core']['damage_px_total'], 'vs origin', rows[f]['vs_route_w_session_export']['px_differing'], flush=True)
        rec[name] = rows
    return rec


def redraw_jobs(session):
    """Reopen, without a window, every copy tools/cert-redraw.ps1 saved mid-redraw, and export it."""
    red = CERT / session / 'out' / 'REDRAW'
    work = CERT / session / 'work'
    steps = json.loads((red / 'steps.json').read_text(encoding='utf-8-sig'))
    std = [{'kind': 'pdf', 'preset': '[High Quality Print]', 'overrides': {}, 'file': 'hqp.pdf'},
           {'kind': 'png', 'ppi': 600, 'file': 'png600.png'}]
    return [{'id': 'REDRAW.' + Path(sv['file']).stem, 'source': fwd(red / sv['file']), 'copy': fwd(work / ('REDRAW.' + sv['file'])),
             'prep': [], 'outDir': fwd(red / Path(sv['file']).stem), 'outputs': std} for sv in steps['saves']]


def measure_redraw(session_dir, render, masks_cache):
    """Everything saved or exported during the redraw stress must draw as the census does: the
    copies saved with VC05 at 3 pt as the direct export, those at 8 pt as edit cycle 1."""
    import routew_measure as m
    red = session_dir / 'out' / 'REDRAW'
    steps = json.loads((red / 'steps.json').read_text(encoding='utf-8-sig'))
    frames = manifest_frames('V')
    core = _core(masks_cache, ('V', 600), session_dir / 'out/V/mask/mask600.png')
    rec = {'steps': len(steps['steps']), 'actions': {}, 'errors': [s['action'] for s in steps['steps'] if str(s['action']).startswith('ERROR')],
           'saves': [], 'live': {}, 'export_log': steps['export'], 'closed': steps['closed']}
    for s in steps['steps']:
        a = str(s['action']).split(' ')[0]
        rec['actions'][a] = rec['actions'].get(a, 0) + 1
    expected = {3.0: session_dir / 'out/V/out', 8.0: session_dir / 'out/ED/ED3-cycle1'}
    for sv in steps['saves']:
        row = {'step': sv['step'], 'vc05_weight': sv['vc05Weight'], 'outputs': {}}
        for f in ('hqp.pdf', 'png600.png'):
            out = red / Path(sv['file']).stem / f
            o, _ = render(out, 600)
            t, _ = render(session_dir / 'out/V/fill' / f, 600)
            row['outputs'][f] = {'fill_core_damage': m.core_damage(o, t, core, frames, 600)['damage_px_total'],
                                 'vs_expected_state': _pair_stats(render, out, expected[float(sv['vc05Weight'])] / f, 600)}
        rec['saves'].append(row)
        print('REDRAW', sv['file'], {k: (v['fill_core_damage'], v['vs_expected_state']['px_differing']) for k, v in row['outputs'].items()}, flush=True)
    # live.pdf and live-600ppi.png since the export moved under lib.run; the first run's names before.
    names = (('hqp.pdf', 'live.pdf'), ('png600.png', 'live-600ppi.png'))
    if not (red / 'live.pdf').exists():
        names = (('hqp.pdf', 'live-hqp.pdf'), ('png600.png', 'live-png600.png'))
    for f, live in names:
        o, _ = render(red / live, 600)
        t, _ = render(session_dir / 'out/V/fill' / f, 600)
        rec['live'][live] = {'fill_core_damage': m.core_damage(o, t, core, frames, 600)['damage_px_total'],
                             'vs_direct_export': _pair_stats(render, red / live, session_dir / 'out/V/out' / f, 600)}
        print('REDRAW', live, rec['live'][live]['fill_core_damage'], rec['live'][live]['vs_direct_export']['px_differing'], flush=True)
    return rec


def ink_bbox(arr, sl, backdrop):
    import numpy
    ink = abs(arr[sl].astype(int) - backdrop) > 32
    ys, xs = numpy.nonzero(ink)
    return [int(ys.min()), int(xs.min()), int(ys.max()), int(xs.max())] if len(ys) else None


def compare_sessions(routew, native, extra, output, gs, work):
    """Route W against the native control, per output and per suite frame, and route W against
    itself in another process."""
    import numpy
    import routew_measure as m
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    render = Renders(gs, work)
    Path(work).mkdir(parents=True, exist_ok=True)
    W, N = CERT / routew, CERT / native
    rec = {'schema': 1, 'routew_session': routew, 'native_session': native, 'reproducibility_session': extra,
           'sources': {}, 'suites': {}, 'reproducibility': {}}
    for key in ('V', 'T', 'C', 'E'):
        rows = {}
        for out in sorted((N / 'out' / key / 'out').iterdir()):
            wout = W / 'out' / key / 'out' / out.name
            if not wout.exists():
                continue
            ppi = output_ppi(out.name)
            a, _ = render(wout, ppi)
            b, _ = render(out, ppi)
            core = m.core_of(m.gray(N / 'out' / key / 'mask' / ('mask%d.png' % ppi)), ppi)
            (a, b, core), trimmed = m.align(a, b, core)
            d = numpy.abs(a.astype(int) - b.astype(int))
            rows[out.name] = {'px_over_64': int((d > 64).sum()), 'px_over_64_in_fill_core': int((core & (d > 64)).sum()),
                              'px_over_64_outside_fill_core': int((~core & (d > 64)).sum()), 'trimmed_px': trimmed}
            print('W vs native', key, out.name, rows[out.name], flush=True)
        rec['sources'][key] = rows
    for rel in SUITES:
        stem = Path(rel).stem
        manifest = REPO / 'tests/indesign-stroke' / Path(rel).parts[0] / 'actual' / (stem + '.manifest.json')
        frames = [dict(f, key=f['name']) for f in json.loads(manifest.read_text(encoding='utf-8'))['frames']]
        a, _ = render(W / 'out/SUITE' / stem / 'png300.png', 300)
        b, _ = render(N / 'out/SUITE' / stem / 'png300.png', 300)
        backdrop = int(b[3, 3])
        em = m.edge_metrics(a, b, frames, 300)
        rows = []
        for f, e in zip(frames, em['frames']):
            sl = m.frame_box(f['bounds'], 300, b.shape)
            wb, nb = ink_bbox(a, sl, backdrop), ink_bbox(b, sl, backdrop)
            rows.append({'key': f['key'], 'center_aligned': 'center' in (f['name'] + ' ' + f.get('variant', '')).lower(),
                         'px_diff': e['px_diff'], 'px_over_64': e['px_over_64'], 'over_64_beyond_band': e['over_64_beyond_band'],
                         'ink_bbox_routew': wb, 'ink_bbox_native': nb,
                         'ink_bbox_max_delta_px': max(abs(x - y) for x, y in zip(wb, nb)) if wb and nb else None})
        rec['suites'][stem] = {'ppi': 300, 'frames': rows}
        print('suite', stem, 'frames beyond band', sum(1 for r in rows if r['over_64_beyond_band']), 'center identical',
              [r['px_diff'] == 0 for r in rows if r['center_aligned']], flush=True)
    if extra:
        X = CERT / extra
        for key in ('V', 'T', 'C', 'E'):
            for out in sorted((W / 'out' / key / 'out').iterdir()):
                other = X / 'out' / key / 'out' / out.name
                if other.exists():
                    rec['reproducibility']['%s/%s' % (key, out.name)] = _pair_stats(render, out, other, output_ppi(out.name))
        diffs = {k: v['px_differing'] for k, v in rec['reproducibility'].items() if v['px_differing']}
        print('reproducibility: outputs', len(rec['reproducibility']), 'differing', diffs, flush=True)
    Path(output).write_bytes((json.dumps(rec, indent=1, ensure_ascii=False) + '\n').encode('utf-8'))
    print('wrote', output)


def prefs_diff(before, after):
    """Compare two tools/jsx/cert-enumerate.jsx snapshots: every readable property of the export,
    place, import, transparency, display, and GPU preferences, and the preset lists. Returns the
    changed properties; an empty list is the hygiene gate."""
    a = json.loads(Path(before).read_text(encoding='utf-8'))
    b = json.loads(Path(after).read_text(encoding='utf-8'))
    changed = [(k, x, y) for k in a['props'] for x, y in zip(a['props'][k], b['props'].get(k, [])) if x != y]
    if [p['name'] for p in a['pdfPresets']] != [p['name'] for p in b['pdfPresets']]:
        changed.append(('pdfExportPresets', 'list', 'list changed'))
    if a['flattenerPresets'] != b['flattenerPresets']:
        changed.append(('flattenerPresets', 'list', 'list changed'))
    return changed


def measure_conversions(session_dir, render, masks_cache):
    """Route W's PostScript and EPS converted to PDF downstream (Acrobat Distiller through its COM
    interface with the High Quality Print job options; Ghostscript pdfwrite), each against its
    fill twin taken through the same conversion. Files: out/CONV/{out,fill}-<source>-<converter>.pdf."""
    import routew_measure as m
    conv = session_dir / 'out' / 'CONV'
    frames = manifest_frames('V')
    core = _core(masks_cache, ('V', 600), session_dir / 'out/V/mask/mask600.png')
    rows = {}
    for out in sorted(conv.glob('out-*.pdf')):
        twin = conv / ('fill-' + out.name[4:])
        o, renderer = render(out, 600)
        t, _ = render(twin, 600)
        st = m.pdf_structure(str(out), frames)
        rows[out.name[4:]] = {'sha256': file_sha(out), 'bytes': out.stat().st_size, 'renderer': renderer,
                              'fill_core': m.core_damage(o, t, core, frames, 600),
                              'structure': {k: v for k, v in st.items() if k != 'frames'},
                              'frames_with_stroke_after_fill_text': [f['key'] for f in st['frames'] if f['stroke_after_fill_text']]}
        print('CONV', out.name, 'damage', rows[out.name[4:]]['fill_core']['damage_px_total'], 'text chars', st['text_chars'], 'images', st['images'], flush=True)
    return rows


def job_log(session_dir):
    jobs = {}
    for line in (session_dir / 'results.jsonl').read_text(encoding='utf-8').splitlines():
        r = json.loads(line)
        jobs[r['id']] = {'error': r['error'], 'prep': r['prep'], 'ms': r['ms'], 'state': r.get('state'), 'placed': r.get('placed'),
                         'outputs': [{k: o[k] for k in ('file', 'kind', 'ms', 'bytes', 'error', 'log')} for o in r['outputs']]}
    return jobs


def measure(session, output, gs, work, label, keys):
    import subprocess
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    session_dir = CERT / session
    if not str(work).isascii():
        raise SystemExit('--work must be an ASCII path')
    Path(work).mkdir(parents=True, exist_ok=True)
    render = Renders(gs, work)
    record = {'schema': 1, 'session': session, 'label': label,
              'user_presets': {k: 'user-defined preset (name kept out of the repository)' for k in user_presets()},
              'ghostscript': subprocess.run([gs, '--version'], capture_output=True, text=True).stdout.strip(),
              'corpus_sha256': {p.name: file_sha(p) for p in sorted(CORPUS.iterdir()) if p.is_file()},
              'jobs': job_log(session_dir), 'sources': {}}
    cache = {}
    for key in keys:
        if key == 'RT':
            record['roundtrip'] = measure_roundtrip(session_dir, render, cache)
        elif key == 'AI':
            record['illustrator'] = measure_illustrator(session_dir, render, cache)
        elif key == 'ED':
            record['editing'] = measure_editing(session_dir, render, cache)
        elif key == 'SUITE':
            record['suites'] = measure_suites(session_dir, render)
        elif key == 'SAVED':
            record['saved'] = measure_saved(session_dir, render, cache)
        elif key == 'REDRAW':
            record['redraw'] = measure_redraw(session_dir, render, cache)
        elif key == 'CONV':
            record['conversions'] = measure_conversions(session_dir, render, cache)
        else:
            record['sources'][key] = measure_source(key, session_dir, render, cache)
            cache = {k: v for k, v in cache.items() if k[0] != key}
    Path(output).write_bytes((json.dumps(record, indent=1, ensure_ascii=False) + '\n').encode('utf-8'))
    print('wrote', output)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='cmd', required=True)
    p = sub.add_parser('plan')
    p.add_argument('session')
    p.add_argument('--families', nargs='+', default=['V', 'T', 'C', 'E', 'RT'])
    p.add_argument('--control', action='store_true', help='the A/B subset (PLAN_CONTROL) instead of the full plan')
    p.add_argument('--spec-name', default='spec.json')
    q = sub.add_parser('measure')
    q.add_argument('session')
    q.add_argument('output', type=Path)
    q.add_argument('--gs', required=True)
    q.add_argument('--work', type=Path, required=True)
    q.add_argument('--label', default='')
    q.add_argument('--keys', nargs='+', default=['V', 'T', 'C', 'E', 'RT'])
    c = sub.add_parser('compare')
    c.add_argument('routew')
    c.add_argument('native')
    c.add_argument('output', type=Path)
    c.add_argument('--extra', default='', help='a second route W session, for reproducibility')
    c.add_argument('--gs', required=True)
    c.add_argument('--work', type=Path, required=True)
    d = sub.add_parser('prefs-diff')
    d.add_argument('before')
    d.add_argument('after')
    args = parser.parse_args(argv)
    if args.cmd == 'prefs-diff':
        changed = prefs_diff(args.before, args.after)
        for c in changed:
            print('CHANGED', *c)
        print('preferences unchanged' if not changed else '%d preference(s) changed' % len(changed))
        return 1 if changed else 0
    if args.cmd == 'compare':
        compare_sessions(args.routew, args.native, args.extra, args.output, args.gs, args.work)
    elif args.cmd == 'plan':
        plan(args.session, args.families, args.control, args.spec_name)
    elif args.cmd == 'measure':
        measure(args.session, args.output, args.gs, args.work, args.label, args.keys)


if __name__ == '__main__':
    sys.exit(main())
