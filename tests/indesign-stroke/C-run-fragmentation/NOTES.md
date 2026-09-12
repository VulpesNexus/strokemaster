# Fixture C: character-run stroke fragmentation

Reproduction notes for run fragmentation. The mechanism is in *docs/STROKE_MECHANISMS.md*, M1, M2, and M4.

## Files

| Path | What it is |
| --- | --- |
| *../corpus/pristine/fixture-pristine.indd* | The supplied fixture, read-only; fixture C is the "Changing any paramater…" frame with 13 single-character overrides |
| *minimal/C-attribute-sweep.indd* | One frame per attribute variant, built by *tools/jsx/build-C-attribute-sweep.jsx* |
| *actual/C-attribute-sweep.pdf*, *actual/C-attribute-sweep-300ppi.png*, *actual/C-attribute-sweep.idml* | Exports |
| *actual/C-attribute-sweep.manifest.json* | Each variant's key, description, application status, bounds, and lines |
| *actual/C-attribute-sweep.probe.md*, *actual/C-attribute-sweep.probe.json* | Paint objects per line for every variant, from *tools/stroke_probe.py* |
| *expected/C-attribute-sweep-expected.pdf*, *expected/C-attribute-sweep-expected-300ppi.png* | The export with each frame's Outside strokes moved before that frame's fills, written by *tools/make_expected.py* |

## Reproduce

1. With InDesign 2026 running: *powershell -NoProfile -File tools\run-jsx.ps1 build-C-attribute-sweep.jsx*
2. Measure: *python tools/stroke_probe.py tests/indesign-stroke/C-run-fragmentation/actual C-attribute-sweep*
3. Expected output: *python tools/make_expected.py tests/indesign-stroke/C-run-fragmentation C-attribute-sweep*

## Setup

Each frame holds "nnnnnnn" in Minion Pro Regular 24 pt with a 4 pt Paper stroke, Outside, Round join, fill Black. Exactly one attribute is overridden, normally on the fourth character. The ligature variants use "nnfinnn" and "nnstnnn", each with its own control frame; the variable-font variants use Bahnschrift, also with a control frame.

## How to read the probe

- `S F S F S F` means the override created separate wax runs. Each run's stroke is painted over the previous run's fill, which produces a seam.
- `S F` means one run and no seam.
- `S S S F F F` means one run whose stroke call and fill call the PDF exporter split into several objects. This happens for language, which gets its own marked content. There is no seam.

The full table of which attributes split a run is in *docs/STROKE_MECHANISMS.md*, M4. The attributes that split a run without any visual effect are the strongest evidence: underline color with underline off (C25), Lowercase on a lowercase letter (C27), an applied condition with hidden indicators (C38), a miter limit under a round join (C08), and an explicit stroke tint of 100 (C45).

## Notes

- C21, character rotation of 10°, drew no rotation in this configuration and is inconclusive.
- The expected output removes every seam in the sweep. The halo outline doesn't change, which shows the stroke geometry itself was never fragmented.
