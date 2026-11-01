"""Build/run the native refusal, stroke-behind core, cross-frame core, miter core, Illustrator contour, and settings tests with MSVC; never load Adobe or write to its SDK."""
import os
from pathlib import Path
import subprocess
import sys


def main():
    repo = Path(__file__).resolve().parents[2]
    out = repo / "build" / "native-refusal"
    out.mkdir(parents=True, exist_ok=True)
    # A fresh environment mapping removes duplicate PATH/Path entries inherited from
    # some supervisors; MSBuild/.NET cannot accept both. Values are never logged.
    env = dict(os.environ)
    # Visual Studio's optional vcpkg initializer uses an unquoted PATH assignment.
    # Start this compiler-only child with Windows tools; vcvars adds the toolchain.
    # This avoids interpreting shell metacharacters in unrelated inherited paths.
    system_root = Path(env["SYSTEMROOT"])
    env["PATH"] = os.pathsep.join(str(p) for p in (
        system_root / "System32", system_root, system_root / "System32/Wbem"))
    vswhere = Path(env["ProgramFiles(x86)".upper()]) / "Microsoft Visual Studio/Installer/vswhere.exe"
    installations = subprocess.check_output(
        [str(vswhere), "-all", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
         "-property", "installationPath"], env=env, text=True,
        creationflags=subprocess.CREATE_NO_WINDOW,
    ).splitlines()
    vcvars = None
    for installation in installations:
        candidate = Path(installation) / "VC/Auxiliary/Build/vcvars64.bat"
        if candidate.is_file():
            vcvars = candidate
            break
    if vcvars is None:
        raise RuntimeError("Visual Studio C++ toolchain not found")
    # Only fixed repository-relative arguments and a discovered, quoted vcvars path
    # enter cmd. Supply a raw Windows command line: list2cmdline's backslash-quote
    # escaping is not cmd.exe syntax for the nested /c program.
    command = (
        f'call "{vcvars}" -vcvars_ver=14.42 >nul && '
        'cl /nologo /EHsc /std:c++17 /W4 /WX /Itests\\native-repaint '
        'tests\\native-repaint\\NativeRepaintTests.cpp '
        '/Febuild\\native-refusal\\NativeRepaintTests.exe '
        '/Fobuild\\native-refusal\\NativeRepaintTests.obj && '
        'build\\native-refusal\\NativeRepaintTests.exe && '
        'cl /nologo /EHsc /std:c++17 /W4 /WX '
        'tests\\native-repaint\\StrokeBehindCoreTests.cpp '
        '/Febuild\\native-refusal\\StrokeBehindCoreTests.exe '
        '/Fobuild\\native-refusal\\StrokeBehindCoreTests.obj && '
        'build\\native-refusal\\StrokeBehindCoreTests.exe && '
        'cl /nologo /EHsc /std:c++17 /W4 /WX '
        'tests\\native-repaint\\CrossFrameCoreTests.cpp '
        '/Febuild\\native-refusal\\CrossFrameCoreTests.exe '
        '/Fobuild\\native-refusal\\CrossFrameCoreTests.obj && '
        'build\\native-refusal\\CrossFrameCoreTests.exe && '
        'cl /nologo /EHsc /std:c++17 /W4 /WX /Iplugin\\third_party\\clipper2\\include '
        'tests\\native-repaint\\MiterCoreTests.cpp plugin\\source\\IseRegionCore.cpp '
        'plugin\\source\\IseClipper2.cpp '
        '/Febuild\\native-refusal\\MiterCoreTests.exe /Fobuild\\native-refusal\\ && '
        'build\\native-refusal\\MiterCoreTests.exe && '
        'cl /nologo /EHsc /std:c++17 /W4 /WX /Iplugin\\third_party\\clipper2\\include '
        'tests\\native-repaint\\IllustratorContourTests.cpp plugin\\source\\IseRegionCore.cpp '
        'plugin\\source\\IseClipper2.cpp '
        '/Febuild\\native-refusal\\IllustratorContourTests.exe /Fobuild\\native-refusal\\ && '
        'build\\native-refusal\\IllustratorContourTests.exe && '
        'cl /nologo /EHsc /std:c++17 /W4 /WX tests\\native-repaint\\SettingsTests.cpp '
        '/Febuild\\native-refusal\\SettingsTests.exe /Fobuild\\native-refusal\\SettingsTests.obj && '
        'build\\native-refusal\\SettingsTests.exe'
    )
    result = subprocess.run(
        'cmd.exe /d /s /c "' + command + '"', cwd=repo, env=env,
        text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        creationflags=subprocess.CREATE_NO_WINDOW,
    )
    (out / "tests.log").write_text(result.stdout, encoding="utf-8")
    print(result.stdout, end="")
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
