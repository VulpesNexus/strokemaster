# run-jsx.ps1 - run one InDesign Stroke Engine build script in the running InDesign 2026.
#
# Usage:
#   powershell -NoProfile -File tools\run-jsx.ps1 build-A-minimal.jsx
#   powershell -NoProfile -File tools\run-jsx.ps1 build-D-faithful.jsx -With ise-faithful-outlines.jsx
#   powershell -NoProfile -File tools\run-jsx.ps1 build-D-hook-test.jsx -With ise-faithful-outlines.jsx,ise-create-outlines-hook.jsx -Engine iseHookTest
#   powershell -NoProfile -File tools\run-jsx.ps1 ise-reexport.jsx -ArgJson '{"docPath":"...","outDir":"...","stem":"N-transparency"}'
#
# -ArgJson merges its properties into ISE_ARGS, next to outRoot, for scripts that act on a
# document named by the caller rather than one they build themselves.
#
# ise-lib.jsx is always loaded first, then the -With libraries in order, then the build
# script. -Engine runs everything in a named persistent ExtendScript engine, which function
# event listeners require. Every global the libraries define is deleted afterward.
#
# The build scripts create their own documents, save them under tests\indesign-stroke\<issue>\
# minimal, export under ...\actual, close them, and restore InDesign's export preferences.
# They never touch documents they did not create.
# Pure ASCII: this file is read in the system codepage.
param(
    [Parameter(Mandatory = $true)][string]$Script,
    [string[]]$With = @(),
    [string]$Engine = '',
    [string]$ArgJson = ''
)
$ErrorActionPreference = 'Stop'

function ConvertTo-JsPath([string]$Path) {
    return (($Path -replace '\\', '/') -replace "'", "\'")
}

$repo = Split-Path -Parent $PSScriptRoot
$jsxDir = Join-Path $PSScriptRoot 'jsx'
$outRoot = ConvertTo-JsPath (Join-Path $repo 'tests\indesign-stroke')

# ISE_ARGS is emitted as a JavaScript object literal. JSON is a subset of that syntax, and
# ExtendScript has no JSON parser of its own, so the serialized form is used directly. JSON
# string escaping matches JavaScript's, so a Windows path survives either separator.
$argsObj = [ordered]@{ outRoot = $outRoot }
if ($ArgJson) {
    $extra = $ArgJson | ConvertFrom-Json
    foreach ($prop in $extra.PSObject.Properties) { $argsObj[$prop.Name] = $prop.Value }
}
$argsLiteral = $argsObj | ConvertTo-Json -Compress

$loads = @("`$.evalFile(new File('" + (ConvertTo-JsPath (Join-Path $jsxDir 'ise-lib.jsx')) + "'));")
foreach ($libName in $With) {
    foreach ($part in ($libName -split ',')) {
        if ($part.Trim()) { $loads += "`$.evalFile(new File('" + (ConvertTo-JsPath (Join-Path $jsxDir $part.Trim())) + "'));" }
    }
}
$buildPath = ConvertTo-JsPath (Join-Path $jsxDir $Script)
$engineLine = ''
if ($Engine) { $engineLine = "#targetengine `"$Engine`"" }

$js = @"
$engineLine
`$.global.ISE_ARGS = $argsLiteral;
var __r = 'no result';
try {
    $($loads -join "`n    ")
    __r = `$.evalFile(new File('$buildPath'));
} catch (e) {
    __r = 'WRAPPER ERROR ' + e.message + ' line ' + e.line;
} finally {
    try { if (`$.global.ISE_HOOK) { `$.global.ISE_HOOK.uninstall(); } } catch (e1) {}
    try { delete `$.global.ISE_LIB; } catch (e2) {}
    try { delete `$.global.ISE_FAITHFUL; } catch (e3) {}
    try { delete `$.global.ISE_SPLIT; } catch (e6) {}
    try { delete `$.global.ISE_HOOK; } catch (e4) {}
    try { delete `$.global.ISE_ARGS; } catch (e5) {}
}
__r;
"@

$app = New-Object -ComObject InDesign.Application.2026
$app.DoScript($js, 1246973031)
