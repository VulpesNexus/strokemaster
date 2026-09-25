# cert-redraw.ps1 - stress InDesign's screen drawing of a route W document, then check that nothing
# temporary reached the document or its exports.
#
#   powershell -NoProfile -File tools\cert-redraw.ps1 -Out <fresh folder> [-Steps 60] [-Seed 7]
#
# Route W mutes a stroke in one adornment pass and restores it in a later one. A screen draw cut
# short between the two could leave a stroke muted in memory, where a save or an export would
# find it. This opens a copy of the frozen victim census in a window and sends short scripting
# calls one at a time (zoom, fit, scroll, display quality, preview mode, minimize and restore
# the document window, and a stroke edit that is undone again), each followed by a random pause
# of 0 to 150 ms, so the next call arrives while the window may still be drawing. Every
# SaveEvery steps, straight after a call and with no pause, it saves a copy. At the end it
# exports from the window, closes it without saving, and writes steps.json. The saved copies
# and the live exports are then measured by tools/certify_routew.py (the REDRAW family).
#
# Run it only in an InDesign session a tool launched, never in the owner's: it opens a window
# and changes that window's view. The owner's other windows and the application frame are not
# touched. Pure ASCII: this file is read in the system codepage.
param(
    [Parameter(Mandatory = $true)][string]$Out,
    [int]$Steps = 60,
    [int]$SaveEvery = 15,
    [int]$Seed = 7
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$source = Join-Path $repo 'tests\certification\corpus\victims.indd'
if (Test-Path -LiteralPath $Out) { throw "Refusing to reuse $Out" }
New-Item -ItemType Directory -Force -Path $Out | Out-Null
$copy = Join-Path $Out 'redraw-source.indd'
Copy-Item -LiteralPath $source -Destination $copy
$js = { param($s) ($s -replace '\\', '/') -replace "'", "\'" }
$copyJs = & $js $copy
$outJs = & $js $Out

# InDesign does not always publish itself in the Running Object Table; COM activation then
# attaches to the running process. Require exactly one, so activation can never launch another.
if (@(Get-Process InDesign -ErrorAction SilentlyContinue).Count -ne 1) { throw 'Require exactly one running InDesign.' }
try { $app = [Runtime.InteropServices.Marshal]::GetActiveObject('InDesign.Application.2026') }
catch { $app = New-Object -ComObject InDesign.Application.2026 }
function Invoke-Id([string]$code) {
    # One line per call: multi-line strings sent through DoScript have returned empty without running.
    return [string]$app.DoScript($code, 1246973031)
}

$name = Invoke-Id "var d = app.open(File('$copyJs'), true); d.name;"
$doc = "app.documents.itemByName('$name')"
$props = Invoke-Id "var w = $doc.layoutWindows[0]; var a = []; var p = w.reflect.properties; for (var i = 0; i < p.length; i++) { try { a.push(p[i].name + '=' + w[p[i].name]); } catch (e) { a.push(p[i].name + '=?'); } } a.join('|');"
$rand = New-Object System.Random($Seed)
$zooms = @(12.5, 25, 50, 66.7, 100, 200, 400, 800, 1600, 3200)
$actions = @(
    { "$doc.layoutWindows[0].zoomPercentage = $($zooms[$rand.Next($zooms.Count)]); 'zoom';" },
    { "$doc.layoutWindows[0].zoom(ZoomOptions.FIT_PAGE); 'fit';" },
    { "var w = $doc.layoutWindows[0]; w.zoomPercentage = 800; w.zoom(ZoomOptions.ZOOM_IN); 'zoom in';" },
    { "$doc.layoutWindows[0].viewDisplaySetting = [ViewDisplaySettings.HIGH_QUALITY, ViewDisplaySettings.TYPICAL, ViewDisplaySettings.OPTIMIZED][$($rand.Next(3))]; 'display';" },
    { "$doc.layoutWindows[0].screenMode = [ScreenModeOptions.PREVIEW_OFF, ScreenModeOptions.PREVIEW_TO_PAGE][$($rand.Next(2))]; 'screen mode';" },
    { "var w = $doc.layoutWindows[0]; w.minimize(); w.restore(); 'minimize restore';" },
    { "var p = $doc.textFrames.itemByName('VC05').parentStory.paragraphs[0]; p.strokeWeight = (p.strokeWeight === 3) ? 8 : 3; 'toggle VC05 ' + p.strokeWeight;" },
    { "var w = $doc.layoutWindows[0]; w.zoomPercentage = 400; w.activePage = $doc.pages[0]; 'page';" }
)
$log = @()
$saves = @()
for ($i = 1; $i -le $Steps; $i++) {
    $a = $actions[$rand.Next($actions.Count)]
    $code = & $a
    $t0 = Get-Date
    try { $r = Invoke-Id $code } catch { $r = 'ERROR ' + $_.Exception.Message }
    $log += [ordered]@{ step = $i; action = $r; ms = [int]((Get-Date) - $t0).TotalMilliseconds }
    if ($i % $SaveEvery -eq 0) {
        $weight = Invoke-Id "var d = $doc; d.saveACopy(File('$outJs/redraw-save-$i.indd')); '' + d.textFrames.itemByName('VC05').parentStory.paragraphs[0].strokeWeight;"
        $saves += [ordered]@{ step = $i; file = "redraw-save-$i.indd"; vc05Weight = [double]$weight }
    } else {
        Start-Sleep -Milliseconds ($rand.Next(151))
    }
}
# Return VC05 to the census value, export from the window, and close it unsaved.
$final = Invoke-Id "var d = $doc; d.textFrames.itemByName('VC05').parentStory.paragraphs[0].strokeWeight = 3; d.layoutWindows[0].zoomPercentage = 400; '' + d.modified;"
# Export from the open window through tools/jsx/ise-export-open.jsx, under lib.run: a PDF export
# with a preset copies that preset into app.pdfExportPreferences (and turns on
# preserveInDesignEditingCapabilities, which the preset object does not even expose), so an
# export made outside lib.run leaves the owner's PDF preferences changed. It writes live.pdf and
# live-600ppi.png and leaves the document as it is.
$argJson = @{ docPath = ($copy -replace '\\', '/'); outDir = ($Out -replace '\\', '/'); stem = 'live'; ppi = 600; eps = 'no'; copy = 'no' } | ConvertTo-Json -Compress
$export = (& (Join-Path $PSScriptRoot 'run-jsx.ps1') ise-export-open.jsx -ArgJson $argJson) -split "`n" | Where-Object { $_ -match '^(pdf|png|ERROR)|restore: fails' } | ForEach-Object { ($_ -split ' \[')[0] }
$export = $export -join '; '
$closed = Invoke-Id "$doc.close(SaveOptions.NO); 'closed';"
$record = [ordered]@{ schema = 1; seed = $Seed; steps = $log; saves = $saves; finalModified = $final; export = $export; closed = $closed; windowProperties = $props }
[IO.File]::WriteAllText((Join-Path $Out 'steps.json'), ($record | ConvertTo-Json -Depth 5), (New-Object Text.UTF8Encoding($false)))
"steps=$Steps saves=$($saves.Count) export=$export closed=$closed"
