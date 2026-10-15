"""Bounded geometry-enabled region target; compile, mock tests, and verify identity.

Never installs, loads the plugin, or writes to the external SDK. Builds the real
topology core from the reviewed freeze input and links it into the region plugin.
"""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
from xml.sax.saxutils import escape as xml_escape

root = Path(__file__).resolve().parent
out = root / 'build/region-native'
sdk = Path(sys.argv[1]).resolve()
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
frozen = {root / 'build/native-prep4-frozen.aip': '45fab742d70bfb12497b3735a1e3c9d4b14dd8dc9195551f0221f1c4297b1c78',
          root / 'build/native-prep5-frozen.aip': '201dd19cb25174c7fb9628fd1bf2769aa41e7d4dd4f2c6b77e5e9d7aea0fc9c7',
          root / 'build/region-prep2-frozen.aip': 'bd6bd27b905285898f72be0df408bc1f935b3e99e8b0d0855e79bb6938a334cb'}
assert all(digest(p) == h for p, h in frozen.items()), 'Frozen prep artifact mismatch'
core_header = root.parents[1] / 'tools/topology_region_core.h'
core_cpp = root.parents[1] / 'tools/topology_region.cpp'
header_sha = digest(core_header)
cpp_sha = digest(core_cpp)
assert header_sha == '97f01eb213e30463bd6161ee2341e3725d284182fadff13c2f2092dc0d17ffa7', 'Core header differs from reviewed freeze input'
boost = sdk.parents[0] / 'Adobe InDesign 2026 SDK/external/dva/third_party/boost_libraries'
if not (boost / 'boost/version.hpp').is_file():
    raise RuntimeError('External Boost headers unavailable')
if not Path(os.environ['ProgramFiles'], 'Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe').is_file():
    raise RuntimeError('Expected MSBuild installation unavailable')
out.mkdir(parents=True, exist_ok=True)
props = ('<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003"><PropertyGroup>'
         f'<ProbeSdkRoot>{xml_escape(str(sdk))}</ProbeSdkRoot>'
         '<VCToolsVersion>14.42.34433</VCToolsVersion>'
         '<WindowsTargetPlatformVersion>10.0.26100.0</WindowsTargetPlatformVersion>'
         f'<RegionBoostRoot>{xml_escape(str(boost))}</RegionBoostRoot>'
         '</PropertyGroup></Project>')
# RegionProbe.vcxproj imports build\inputs.props relative to the project directory,
# the same shared file the probe build script writes for Probe.vcxproj.
(root / 'build/inputs.props').write_text(props, encoding='utf-8')
env = {k.upper(): v for k, v in os.environ.items()}
def run(name, args, timeout=120):
    result = subprocess.run(args, cwd=out, env=env, capture_output=True, timeout=timeout,
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
# The output backend and the stroke-behind and text-behind transforms carry their own mock suites.
for project, folder in (('RegionSdkOutputTests', 'region-sdk-output-tests'), ('StrokeBehindTests', 'stroke-behind-tests'),
                        ('TextBehindTests', 'text-behind-tests')):
    run(project + '-compile', [str(msbuild), str(root / (project + '.vcxproj')), '/p:Configuration=Release',
                               '/p:Platform=x64', '/m:1', '/v:minimal', '/nologo'])
    run(project, [str(root / 'build' / folder / (project + '.exe'))])
binary = out / 'StrokeMasterRegionProbe.aip'
assert (out / 'plugin.pipl').read_bytes() in binary.read_bytes(), 'PiPL resource missing'
assert b'StrokeMasterRegionProbe' in (out / 'plugin.pipl').read_bytes(), 'Wrong PiPL identity'
assert all(digest(p) == h for p, h in frozen.items()), 'Frozen prep artifact changed during build'
assert digest(core_header) == header_sha and digest(core_cpp) == cpp_sha, 'Core changed during build'
record = {'status': 'PROPOSED_NATIVE_ROUTE', 'plugin': 'StrokeMasterRegionProbe', 'build': 'region-prep-16',
          'sha256': digest(binary), 'bytes': binary.stat().st_size, 'geometryEnabled': True, 'coreLinked': True,
          'hostTested': False, 'piplEmbedded': True, 'prep4AndPrep5Unchanged': True,
          'coreHeaderSha256': header_sha, 'coreCppSha256': cpp_sha,
          'supportedEnvelope': 'marked analytic fixture only; all other input refused before staging',
          'sourceSha256': {n: digest(root / n) for n in ['RegionProbe.cpp', 'RegionProbeTests.cpp', 'RegionProbe.vcxproj',
                                                        'IllustratorContour.h', 'IllustratorSdkContour.h',
                                                        'TextBehind.h', 'StrokeBehind.h', 'TextBehindTests.cpp', 'NativeMiter.h',
                                                        'NativeRegion.h']}}
(out / 'artifact.json').write_text(json.dumps(record, indent=2) + '\n', encoding='utf-8')
print(json.dumps(record))
