"""Compile the standalone experiment with existing MSVC and Boost headers only."""
import argparse
import os
from pathlib import Path
import subprocess
import sys


def main():
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--boost-root", type=Path, required=True)
    parser.add_argument("--msvc-root", type=Path, required=True,
                        help="Existing VC/Tools/MSVC/<version> directory")
    parser.add_argument("--windows-sdk", type=Path, required=True,
                        help="Existing Windows Kits/10 directory")
    parser.add_argument("--sdk-version", default="10.0.26100.0")
    parser.add_argument("--floating-overlay", action="store_true",
                        help="Measure Boost's floating overlay mode without integer rescaling")
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[1]
    output = repo / "tests/geometry/topology_build"
    compiler = args.msvc_root / "bin/Hostx64/x64/cl.exe"
    includes = [args.boost_root, args.msvc_root / "include"]
    includes += [args.windows_sdk / "Include" / args.sdk_version / name
                 for name in ("ucrt", "shared", "um")]
    libraries = [args.msvc_root / "lib/x64"]
    libraries += [args.windows_sdk / "Lib" / args.sdk_version / name / "x64"
                  for name in ("ucrt", "um")]
    for path in [compiler, args.boost_root / "boost/geometry.hpp", *includes, *libraries]:
        if not path.exists():
            parser.error(f"Missing installed dependency: {path}")
    output.mkdir(exist_ok=True)
    environment = {k.upper(): v for k, v in os.environ.items()}
    environment["INCLUDE"] = os.pathsep.join(map(str, includes))
    environment["LIB"] = os.pathsep.join(map(str, libraries))
    command = [str(compiler), "/nologo", "/std:c++17", "/EHsc", "/O2", "/W3", "/utf-8",
               "/DBOOST_BIND_GLOBAL_PLACEHOLDERS", str(repo / "tools/topology_region.cpp"),
               f"/Fo{output / 'topology_region.obj'}", f"/Fe{output / 'topology_region.exe'}"]
    if args.floating_overlay:
        command.insert(1, "/DBOOST_GEOMETRY_NO_ROBUSTNESS")
    result = subprocess.run(command, cwd=output, env=environment, capture_output=True)
    log = result.stdout + result.stderr
    (output / "compile.log").write_bytes(log)
    lines = log.decode("utf-8", errors="replace").splitlines()
    diagnostics = [line for line in lines if ": warning " in line or ": error " in line]
    print("\n".join(diagnostics) if diagnostics else "No compiler warnings or errors.")
    print("Full compiler log:", output / "compile.log")
    result.check_returncode()
    print(output / "topology_region.exe")


if __name__ == "__main__":
    main()
