"""Bounded registration-only SDK target; no install, host calls, or SDK writes."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parent
out = root / 'build/region-registration'
sdk = Path(sys.argv[1]).resolve()
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
frozen = {root / 'build/native-prep4-frozen.aip': '45fab742d70bfb12497b3735a1e3c9d4b14dd8dc9195551f0221f1c4297b1c78',
          root / 'build/native-prep5-frozen.aip': '201dd19cb25174c7fb9628fd1bf2769aa41e7d4dd4f2c6b77e5e9d7aea0fc9c7',
          root / 'Probe.cpp': '407c48cdc170f602b04edb8b204ac649b8cf3f5588373a22b3da843e02e68cbb'}
assert all(digest(p) == h for p, h in frozen.items()), 'Frozen prep source/artifact mismatch'
project = (root / 'Probe.vcxproj').read_text(encoding='utf-8')
project = project.replace('ProbeTests', 'RegionProbeTests').replace('StrokeMasterProposedProbe', 'StrokeMasterRegionProbe')
project = project.replace('Include="Probe.cpp"', 'Include="RegionProbe.cpp"')
project = project.replace('\\build\\</OutDir>', '\\build\\region-registration\\</OutDir>')
(root / 'RegionProbe.vcxproj').write_text(project, encoding='utf-8')
out.mkdir(parents=True, exist_ok=True)
env = {k.upper(): v for k, v in os.environ.items()}
def run(name, args):
    result = subprocess.run(args, cwd=out, env=env, capture_output=True, timeout=60,
                            creationflags=subprocess.CREATE_NO_WINDOW)
    (out / (name + '.log')).write_bytes(result.stdout + result.stderr)
    print((result.stdout + result.stderr).decode('utf-8', errors='replace'))
    result.check_returncode()
run('pipl', [sys.executable, '-B', str(sdk / 'tools/pipl/create_pipl.py'), '-input',
             json.dumps([{'name': 'StrokeMasterRegionProbe', 'entry_point': 'PluginMain'}])])
msbuild = Path(os.environ['ProgramFiles']) / 'Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe'
args = [str(msbuild), str(root / 'RegionProbe.vcxproj'), '/p:Configuration=Release', '/p:Platform=x64', '/m:1', '/v:minimal', '/nologo']
run('compile', args)
run('test-compile', args + ['/p:RegionProbeTests=true'])
run('tests', [str(out / 'RegionProbeTests.exe')])
binary = out / 'StrokeMasterRegionProbe.aip'
assert (out / 'plugin.pipl').read_bytes() in binary.read_bytes(), 'PiPL resource missing'
assert b'StrokeMasterRegionProbe' in (out / 'plugin.pipl').read_bytes(), 'Wrong PiPL identity'
assert all(digest(p) == h for p, h in frozen.items()), 'Frozen prep source/artifact changed'
record = {'status': 'PROPOSED_REGISTRATION_ONLY', 'plugin': 'StrokeMasterRegionProbe', 'build': 'region-prep-1',
          'sha256': digest(binary), 'bytes': binary.stat().st_size, 'geometryEnabled': False, 'coreLinked': False,
          'hostTested': False, 'piplEmbedded': True, 'prep4AndPrep5Unchanged': True,
          'sourceSha256': {n: digest(root / n) for n in ['RegionProbe.cpp', 'RegionProbeTests.cpp', 'RegionProbe.vcxproj']}}
(out / 'artifact.json').write_text(json.dumps(record, indent=2) + '\n', encoding='utf-8')
print(json.dumps(record))
