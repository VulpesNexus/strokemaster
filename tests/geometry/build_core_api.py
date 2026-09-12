"""Compile/link the array API test with the CLI excluded; existing tools only."""
import argparse
import os
from pathlib import Path
import subprocess
import sys


def main():
    sys.stdout.reconfigure(encoding="utf-8",errors="replace")
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--boost-root",type=Path,required=True)
    parser.add_argument("--msvc-root",type=Path,required=True)
    parser.add_argument("--windows-sdk",type=Path,required=True)
    parser.add_argument("--sdk-version",default="10.0.26100.0")
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[2]
    output=root/"tests/geometry/topology_build/core_api"
    includes=[args.boost_root,args.msvc_root/"include"]
    includes += [args.windows_sdk/"Include"/args.sdk_version/n for n in ("ucrt","shared","um")]
    libs=[args.msvc_root/"lib/x64"]
    libs += [args.windows_sdk/"Lib"/args.sdk_version/n/"x64" for n in ("ucrt","um")]
    compiler=args.msvc_root/"bin/Hostx64/x64/cl.exe"
    for path in [compiler,*includes,*libs]:
        if not path.exists(): parser.error("Missing installed dependency: "+str(path))
    output.mkdir(exist_ok=True)
    env={k.upper():v for k,v in os.environ.items()}
    env["INCLUDE"]=os.pathsep.join(map(str,includes))
    env["LIB"]=os.pathsep.join(map(str,libs))
    command=[str(compiler),"/nologo","/std:c++17","/EHsc","/O2","/W3","/utf-8",
             "/DISE_TOPOLOGY_CORE_ONLY","/showIncludes",str(root/"tools/topology_region.cpp"),
             str(root/"tests/geometry/topology_core_smoke.cpp"),f"/Fe{output/'topology_core_smoke.exe'}"]
    result=subprocess.run(command,cwd=output,env=env,capture_output=True)
    log=result.stdout+result.stderr
    (output/"compile.log").write_bytes(log)
    lines=log.decode("utf-8",errors="replace").splitlines()
    print("\n".join(line for line in lines if ": error " in line))
    result.check_returncode()
    if any("property_tree" in line or "json_parser" in line for line in lines):
        raise AssertionError("JSON dependency leaked into core-only compilation")
    print("Core-only build: zero JSON/property-tree includes; warnings:",sum(": warning " in line for line in lines))
    subprocess.run([str(output/"topology_core_smoke.exe")],check=True)


if __name__=="__main__":
    main()
