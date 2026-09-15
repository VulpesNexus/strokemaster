"""Compile/run isolated SDK-free scaffold mocks; never build or load the AIP."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parent
output = root / "build" / "topology-adapter-tests"
real_core = "--real-core" in sys.argv[1:]
candidate_sha = None
if "--candidate-sha256" in sys.argv:
    candidate_sha = sys.argv[sys.argv.index("--candidate-sha256") + 1]
if real_core:
    output = root / "build" / "topology-adapter-real-core-tests"
programs = Path(os.environ["ProgramFiles(x86)"])
vswhere = programs / "Microsoft Visual Studio/Installer/vswhere.exe"
installations = subprocess.check_output(
    [str(vswhere), "-all", "-products", "*", "-requires", "Microsoft.Component.MSBuild",
     "-property", "installationPath"], text=True, timeout=10).splitlines()
msvc = next(Path(p) / "VC/Tools/MSVC/14.42.34433" for p in installations
            if (Path(p) / "VC/Tools/MSVC/14.42.34433/bin/Hostx64/x64/cl.exe").exists())
sdk = programs / "Windows Kits/10"
version = "10.0.26100.0"
includes = [msvc / "include"] + [sdk / "Include" / version / p for p in ("ucrt", "shared", "um")]
libraries = [msvc / "lib/x64"] + [sdk / "Lib" / version / p / "x64" for p in ("ucrt", "um")]
for dependency in includes + libraries:
    if not dependency.is_dir():
        raise RuntimeError(f"Missing installed dependency: {dependency}")
frozen = root / "build/native-prep4-frozen.aip"
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
before = digest(frozen)
core_header = root.parents[1] / "tools/topology_region_core.h"
header_before = digest(core_header)
core_cpp = root.parents[1] / "tools/topology_region.cpp"
cpp_before = digest(core_cpp)
if real_core and (header_before != "97f01eb213e30463bd6161ee2341e3725d284182fadff13c2f2092dc0d17ffa7" or
                  cpp_before != (candidate_sha or "f4460f15bb624b6fe0e4383380ca30871172339e8e9d234f5fbdfde9583038ce")):
    raise RuntimeError("Real core differs from reviewed freeze input; no build performed")
if before != "45fab742d70bfb12497b3735a1e3c9d4b14dd8dc9195551f0221f1c4297b1c78":
    raise RuntimeError("Unexpected frozen artifact; no build performed")
output.mkdir(parents=True, exist_ok=True)
env = {k.upper(): v for k, v in os.environ.items()}
env["INCLUDE"] = os.pathsep.join(map(str, includes))
env["LIB"] = os.pathsep.join(map(str, libraries))
exe = output / "TopologyAdapterTests.exe"
command = [str(msvc / "bin/Hostx64/x64/cl.exe"), "/nologo", "/std:c++17", "/EHsc", "/MD", "/W4", "/WX",
           "/utf-8", str(root / "TopologyAdapterTests.cpp"), f"/Fo{output / 'TopologyAdapterTests.obj'}", f"/Fe{exe}"]
if real_core:
    boost = root.parents[3] / "Adobe SDK/Adobe InDesign 2026 SDK/external/dva/third_party/boost_libraries"
    if not (boost / "boost/version.hpp").is_file():
        raise RuntimeError("External Boost headers unavailable")
    core_object = output / "topology_region.obj"
    compile_core = [str(msvc / "bin/Hostx64/x64/cl.exe"), "/nologo", "/std:c++17", "/EHsc", "/MD", "/W3",
                    "/O2", "/utf-8", "/DISE_TOPOLOGY_CORE_ONLY", f"/I{boost}", "/c", str(core_cpp), f"/Fo{core_object}"]
    built = subprocess.run(compile_core, cwd=output, env=env, capture_output=True, timeout=60,
                           creationflags=subprocess.CREATE_NO_WINDOW)
    (output / "core-compile.log").write_bytes(built.stdout + built.stderr)
    built.check_returncode()
    command.extend(["/DPROBE_REAL_CORE_TEST", str(core_object)])
for label, command in [("compile", command), ("tests", [str(exe)])]:
    result = subprocess.run(command, cwd=output, env=env, capture_output=True, timeout=60,
                            creationflags=subprocess.CREATE_NO_WINDOW)
    (output / f"{label}.log").write_bytes(result.stdout + result.stderr)
    print((result.stdout + result.stderr).decode("utf-8", errors="replace"))
    result.check_returncode()
if digest(frozen) != before:
    raise RuntimeError("Frozen artifact unexpectedly changed")
if digest(core_header) != header_before:
    raise RuntimeError("Core header changed during build; API result is not a stable snapshot")
if digest(core_cpp) != cpp_before:
    raise RuntimeError("Core implementation changed during build")
record = {"status": "CANDIDATE_REAL_CORE_ANALYTIC_PASS" if real_core else "MOCK_SCAFFOLD_ONLY", "realKernelAdmitted": False, "adobeSdkUsed": False,
          "realCoreLinked": real_core, "coreCppSha256": cpp_before,
          "runtimeLibrary": "MD", "coreHeaderSha256": header_before,
          "nativeIntegration": False, "frozenAipUnchanged": True, "frozenAipSha256": before,
          "sourceSha256": {name: digest(root / name) for name in ("TopologyAdapter.h", "TopologyAdapterTests.cpp")},
          "testExecutableSha256": digest(exe)}
(output / "result.json").write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
print(json.dumps(record))
