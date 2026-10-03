// Pending native control: evaluate in InDesign 2026 through its Scripts panel.
// Creates one hidden disposable document; never opens, quits, or resizes the host.
// Results are a native-input/export record, not a geometry acceptance verdict.
(function () {
    var here = File($.fileName).parent, previousLib = $.global.ISE_LIB;
    var previousCases = $.global.ISE_MITER_CASES, previousArgs = $.global.ISE_ARGS;
    var lib = null, report = {schema:1, status:'NOT_EXECUTED', nativeGeometryVerdict:'NOT_MEASURED',
        cases:[], artifacts:[], limitations:['PDF and PNG still require independent geometry and renderer measurement.',
        'Closed two-anchor rejection is recorded; it does not authorize dropping font contours.']};
    var dir = null, runLog = '', doc = null;
    // InDesign 21.6 reads 50.01 back as 50.0100000000002; a deviation this small is the
    // host's double round trip, not a changed specimen.
    var READBACK_TOLERANCE = 1e-9;
    // Preferences this probe assigns. One it cannot read back is never excused.
    var WRITTEN = {pdfExportPreferences:{viewPDF:1, pageRange:1, exportReaderSpreads:1, exportAsSinglePages:1},
        pngExportPreferences:{exportResolution:1, pngQuality:1, transparentBackground:1, antiAlias:1,
            pngColorSpace:1, pngExportRange:1, exportingSpread:1, useDocumentBleeds:1, simulateOverprint:1}};
    // A preference unreadable at snapshot time, and not assigned here, cannot have been changed
    // by this probe; it is recorded. Anything else in an unverified list fails the harness.
    function unverifiedProblems(log, excused) {
        var lines = log.split(/\r?\n/), problems = [];
        for (var i = 0; i < lines.length; i++) {
            // No capture groups: InDesign 21.6's ExtendScript returned undefined for the first
            // group of /^(\d+)(?: \[(.*?)\])?.../ while Node filled it.
            var at = lines[i].indexOf(' unverified='), mark = lines[i].indexOf(' restore: ');
            if (at < 0 || mark < 0 || !/^\w+ restore: /.test(lines[i])) { continue; }
            var owner = lines[i].slice(0, mark), rest = lines[i].slice(at + 12), digits = rest.match(/^\d+/);
            var tail = digits ? rest.slice(digits[0].length) : null, cut = tail ? tail.indexOf('] adjusted=[') : -1;
            var list = null;
            if (tail === '' || (tail && tail.indexOf(' adjusted=[') === 0)) { list = ''; }
            else if (tail && tail.indexOf(' [') === 0) {
                if (cut >= 0) { list = tail.slice(2, cut); }
                else if (tail.charAt(tail.length - 1) === ']') { list = tail.slice(2, tail.length - 1); }
            }
            if (list === null) { problems.push(owner + ': unparsed restore line'); continue; }
            var items = list ? list.split('; ') : [];
            if (items.length !== Number(digits[0])) { problems.push(owner + ': unverified count mismatch'); continue; }
            for (var k = 0; k < items.length; k++) {
                var name = items[k].split(':')[0];
                if (/verification failed/.test(items[k]) || (WRITTEN[owner] && WRITTEN[owner][name])) {
                    problems.push(owner + '.' + items[k]);
                } else { excused.push(owner + '.' + items[k]); }
            }
        }
        return problems;
    }
    function write(file, text) {
        file.encoding = 'UTF-8'; file.lineFeed = 'Unix';
        if (file.exists) { throw Error('Refusing existing evidence file'); }
        if (!file.open('w')) { throw Error('Evidence open failed'); }
        try { if (file.write(text) === false) { throw Error('Evidence write failed'); } }
        finally { if (file.close() === false) { throw Error('Evidence close failed'); } }
    }
    function output(name) { return new File(dir.fsName + '/' + name); }
    function verify(name) {
        var f = output(name);
        if (!f.exists || f.length <= 0) { throw Error('Missing or empty artifact ' + name); }
        report.artifacts.push({name:name, bytes:f.length});
    }
    try {
        if (!/^21\./.test(String(app.version))) { throw Error('Require InDesign 2026'); }
        $.global.ISE_ARGS = {};
        $.evalFile(new File(here.fsName + '/ise-lib.jsx')); lib = $.global.ISE_LIB;
        $.evalFile(new File(here.fsName + '/miter-host-cases.jsx'));
        var cases = $.global.ISE_MITER_CASES;
        var base = new Folder(here.parent.parent.fsName + '/build');
        if (!base.exists || !base.create()) { if (!base.exists) { throw Error('Cannot create build directory'); } }
        dir = new Folder(base.fsName + '/miter-controls-' + new Date().getTime());
        if (dir.exists || !dir.create()) { throw Error('Require a new evidence directory'); }
        report.hostVersion = String(app.version);
        report.fixtureUnits = 'pt'; report.pageSize = [100,100]; report.origin = [35,50];
        runLog = lib.run(function () {
            doc = lib.newDoc(100,100);
            // The shared builder adds a gray backdrop; the controls use bare white paper.
            var bg = doc.pages[0].rectangles[0]; bg.locked = false; bg.remove();
            var joins = {miter:EndJoin.MITER_END_JOIN, bevel:EndJoin.BEVEL_END_JOIN, round:EndJoin.ROUND_END_JOIN};
            var caps = {butt:EndCap.BUTT_END_CAP, round:EndCap.ROUND_END_CAP, projecting:EndCap.PROJECTING_END_CAP};
            for (var i=0; i<cases.length; i++) {
                var c=cases[i], row={id:c.id, page:i+1, requested:c, state:'NOT_CREATED'};
                report.cases.push(row);
                var page=i===0?doc.pages[0]:doc.pages.add(), p=null, phase='geometry';
                page.marginPreferences.properties={top:0,left:0,bottom:0,right:0};
                try {
                    p=c.closed?page.polygons.add():page.graphicLines.add(); p.name=c.id;
                    var pts=[];
                    for (var j=0;j<c.points.length;j++) { pts.push([35+c.points[j][0],50-c.points[j][1]]); }
                    p.paths[0].entirePath=pts;
                    p.paths[0].pathType=c.closed?PathType.CLOSED_PATH:PathType.OPEN_PATH;
                    var readPoints=p.paths[0].entirePath, anchors=[];
                    for(var rp=0; rp<readPoints.length; rp++) {
                        anchors.push(readPoints[rp][0] instanceof Array ? readPoints[rp][1] : readPoints[rp]);
                    }
                    var deviation=anchors.length===pts.length?0:null;
                    for(var ap=0; deviation!==null && ap<anchors.length; ap++) {
                        if(!anchors[ap] || anchors[ap].length!==2 || typeof anchors[ap][0]!=='number' ||
                            typeof anchors[ap][1]!=='number') { deviation=null; break; }
                        deviation=Math.max(deviation,Math.abs(anchors[ap][0]-pts[ap][0]),Math.abs(anchors[ap][1]-pts[ap][1]));
                    }
                    if(deviation!==null && !isFinite(deviation)) { deviation=null; }
                    row.readbackDeviation=deviation;
                    row.geometryMatches=deviation!==null && deviation<=READBACK_TOLERANCE;
                    if(!row.geometryMatches) {
                        row.state='HOST_INPUT_NORMALIZED'; row.readbackGeometry=readPoints;
                        p.remove(); continue;
                    }
                    phase='style';
                    p.fillColor=doc.swatches.itemByName('None'); p.strokeColor=doc.swatches.itemByName('Black');
                    p.strokeWeight=c.width; p.strokeAlignment=StrokeAlignment.CENTER_ALIGNMENT;
                    p.endJoin=joins[c.join]; p.endCap=caps[c.cap]; p.miterLimit=c.limit;
                    row.readback={entirePath:p.paths[0].entirePath, pathType:String(p.paths[0].pathType),
                        join:String(p.endJoin), cap:String(p.endCap), width:p.strokeWeight,
                        alignment:String(p.strokeAlignment), limit:p.miterLimit};
                    if (p.paths[0].pathType !== (c.closed?PathType.CLOSED_PATH:PathType.OPEN_PATH) ||
                        p.endJoin !== joins[c.join] || p.endCap !== caps[c.cap] ||
                        p.strokeWeight !== c.width || p.miterLimit !== c.limit || p.strokeAlignment !== StrokeAlignment.CENTER_ALIGNMENT) { throw Error('Requested style did not survive readback'); }
                    row.state='CREATED';
                } catch(e) {
                    if(phase==='style') { throw e; }
                    row.state='HOST_INPUT_REFUSED'; row.error=String(e.message);
                    if(p && p.isValid) { p.remove(); }
                }
            }
            if (report.cases.length !== 42) { throw Error('Incomplete control census'); }
            doc.save(output('controls.indd')); verify('controls.indd');
            var pdf=app.pdfExportPreferences;
            if(pdf.useSecurity) { throw Error('Security-enabled PDF preferences require a separate control session'); }
            pdf.viewPDF=false; pdf.pageRange=PageRange.ALL_PAGES; pdf.exportReaderSpreads=false;
            pdf.exportAsSinglePages=false;
            // Export through preferences so a preset cannot override viewPDF=false.
            report.pdfPreferences={standards:String(pdf.standardsCompliance),compatibility:String(pdf.acrobatCompatibility),
                colorSpace:String(pdf.pdfColorSpace),viewPDF:pdf.viewPDF};
            doc.exportFile(ExportFormat.PDF_TYPE, output('controls.pdf'), false);
            verify('controls.pdf');
            var png=app.pngExportPreferences;
            png.exportResolution=1200; png.pngQuality=PNGQualityEnum.MAXIMUM;
            png.transparentBackground=false; png.antiAlias=true; png.pngColorSpace=PNGColorSpaceEnum.RGB;
            png.pngExportRange=PNGExportRangeEnum.EXPORT_ALL; png.exportingSpread=false;
            png.useDocumentBleeds=false; png.simulateOverprint=false;
            doc.exportFile(ExportFormat.PNG_FORMAT, output('controls.png'), false);
            var images=dir.getFiles('*.png');
            if(images.length !== cases.length) { throw Error('Expected one PNG per control page'); }
            for(var im=0; im<images.length; im++) {
                if(images[im].length<=0) { throw Error('Empty PNG'); }
                report.artifacts.push({name:images[im].name,bytes:images[im].length});
            }
            report.status='HOST_RECORDED';
        });
        report.readbackTolerance = READBACK_TOLERANCE;
        report.unreadablePreferences = [];
        report.unverifiedProblems = unverifiedProblems(runLog, report.unreadablePreferences);
        report.harnessPassed = report.status==='HOST_RECORDED' && !report.unverifiedProblems.length &&
            !/ERROR |close failed:|fails=[1-9]|diff=[1-9]|\(CHANGED\)/.test(runLog);
        if(!report.harnessPassed) { report.status='HOST_HARNESS_FAILED'; }
        report.log=runLog;
        write(output('controls.json'),lib.toJSON(report));
        return lib.toJSON({status:report.status,passed:report.harnessPassed,harnessPassed:report.harnessPassed,
            nativeGeometryVerdict:report.nativeGeometryVerdict,outputFolder:dir.name,cases:report.cases.length});
    } catch(e2) {
        report.status='HOST_HARNESS_FAILED'; report.harnessPassed=false; report.error=String(e2.message);
        if(dir && lib) { try { write(output('failure.json'),lib.toJSON(report)); } catch(e3) {} }
        return 'ERROR: ' + report.error;
    } finally {
        if(previousLib===undefined) { delete $.global.ISE_LIB; } else { $.global.ISE_LIB=previousLib; }
        if(previousCases===undefined) { delete $.global.ISE_MITER_CASES; } else { $.global.ISE_MITER_CASES=previousCases; }
        if(previousArgs===undefined) { delete $.global.ISE_ARGS; } else { $.global.ISE_ARGS=previousArgs; }
    }
})();
