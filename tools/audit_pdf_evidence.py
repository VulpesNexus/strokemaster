"""Independent read-only PDF state census; no dependency on stroke_probe's parser.

Run with the repository Python environment. Text shows are counted, not called wax runs.
No PDF is modified. Form invocations inherit the caller's graphics state.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path

import pikepdf


def census(path):
    shows, paints = [], Counter()
    with pikepdf.open(path) as pdf:
        def walk(stream, resources, state, route, ancestors):
            stack = []
            for operands, operator in pikepdf.parse_content_stream(stream):
                op = str(operator)
                paints[op] += 1
                if op == 'q':
                    stack.append(state.copy())
                elif op == 'Q':
                    if not stack:
                        raise ValueError('Unbalanced graphics restore')
                    state = stack.pop()
                elif op in ('w', 'j', 'J', 'M', 'Tr'):
                    state[op] = float(operands[0])
                elif op == 'gs':
                    gs = resources.get('/ExtGState', {}).get(operands[0], {})
                    for key, dest in [('/LW', 'w'), ('/LJ', 'j'), ('/LC', 'J'), ('/ML', 'M')]:
                        if key in gs:
                            state[dest] = float(gs[key])
                elif op in ('TJ', 'Tj', "'", '"'):
                    shows.append(dict(route=route, **state))
                elif op == 'Do':
                    form = resources.get('/XObject', {}).get(operands[0])
                    if form is not None and form.get('/Subtype') == '/Form':
                        identity = form.objgen
                        if identity in ancestors:
                            raise ValueError('Recursive form graph')
                        walk(form, form.get('/Resources', resources), state.copy(),
                             route + '/' + str(operands[0]), ancestors + [identity])
            if stack:
                raise ValueError('Unbalanced graphics save')

        for i, page in enumerate(pdf.pages):
            walk(page, page.Resources, {'w': 1, 'j': 0, 'J': 0, 'M': 10, 'Tr': 0}, str(i), [])
        return {
            'file': Path(path).name,
            'sha256': hashlib.sha256(Path(path).read_bytes()).hexdigest(),
            'pages': len(pdf.pages),
            'text_shows': len(shows),
            'text_render_modes': dict(Counter(str(int(s['Tr'])) for s in shows)),
            'stroke_text_states': [dict(w=k[0], j=k[1], M=k[2], count=n)
                                   for k, n in sorted(Counter((s['w'], s['j'], s['M'])
                                       for s in shows if s['Tr'] in (1, 2, 5, 6)).items())],
            'text_mode_order': [int(s['Tr']) for s in shows],
            'operators': dict(sorted(paints.items())),
        }


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pdf', nargs='+', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    result = [census(path) for path in args.pdf]
    encoded = json.dumps(result, indent=2) + '\n'
    if args.output:
        args.output.write_text(encoded, encoding='utf-8')
    else:
        print(encoded, end='')
