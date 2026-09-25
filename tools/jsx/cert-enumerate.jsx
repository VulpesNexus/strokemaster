// cert-enumerate.jsx - list the export, print, and flattener options this InDesign exposes.
// Requires ise-lib.jsx. Argument: ISE_ARGS.out, the JSON file to write.
//
// The route W certification matrix is built from what the installed host offers, not from
// assumed preset or option names. This script changes nothing: it reads the PDF export
// presets (with each one's compatibility, standard, color conversion, and flattener), the
// flattener presets, the enumerations the matrix varies, and the property names of the
// export and print preference objects.
(function () {
    var lib = $.global.ISE_LIB;
    var out = { version: app.version, pdfPresets: [], flattenerPresets: [], enums: {}, props: {} };
    function tryRead(o, k) { try { return lib.str(o[k]); } catch (e) { return '<' + e.message + '>'; } }
    var log = lib.run(function () {
        var i, p, keys = ['acrobatCompatibility', 'standardsCompliance', 'pdfColorSpace', 'pdfDestinationProfile',
            'effectivePDFDestinationProfile', 'pdfMarkType', 'appliedFlattenerPreset', 'subsetFontsBelow',
            'exportLayers', 'includeICCProfiles', 'colorBitmapSampling', 'compressTextAndLineArt', 'simulateOverprint'];
        for (i = 0; i < app.pdfExportPresets.length; i++) {
            p = app.pdfExportPresets[i];
            var rec = { name: p.name };
            for (var k = 0; k < keys.length; k++) { rec[keys[k]] = tryRead(p, keys[k]); }
            out.pdfPresets.push(rec);
        }
        for (i = 0; i < app.flattenerPresets.length; i++) {
            p = app.flattenerPresets[i];
            out.flattenerPresets.push({ name: p.name, rasterVectorBalance: tryRead(p, 'rasterVectorBalance'),
                lineArtAndTextResolution: tryRead(p, 'lineArtAndTextResolution'), convertAllTextToOutlines: tryRead(p, 'convertAllTextToOutlines'),
                convertAllStrokesToOutlines: tryRead(p, 'convertAllStrokesToOutlines'), clipComplexRegions: tryRead(p, 'clipComplexRegions') });
        }
        var enumNames = ['AcrobatCompatibility', 'PDFXStandards', 'PDFColorSpace', 'PNGColorSpaceEnum', 'PNGQualityEnum',
            'PostScriptLevels', 'ColorOutputModes', 'EPSColorSpace', 'PreviewTypes', 'DataFormat', 'FlattenerLevel', 'BlendingSpace'];
        for (i = 0; i < enumNames.length; i++) {
            var e = $.global[enumNames[i]], names = [];
            if (e && e.reflect) {
                var ps = e.reflect.properties;
                for (var j = 0; j < ps.length; j++) { if (ps[j].name !== '__proto__' && ps[j].name !== 'reflect') { names.push(ps[j].name); } }
            }
            out.enums[enumNames[i]] = names;
        }
        var objs = { pdfExportPreferences: app.pdfExportPreferences, pngExportPreferences: app.pngExportPreferences,
            epsExportPreferences: app.epsExportPreferences, pdfPlacePreferences: app.pdfPlacePreferences,
            epsImportPreferences: app.epsImportPreferences, transparencyPreferences: app.transparencyPreferences,
            displayPerformancePreferences: app.displayPerformancePreferences };
        try { objs.gpuPerformancePreferences = app.gpuPerformancePreferences; } catch (eg) { out.props.gpuPerformancePreferences = '<' + eg.message + '>'; }
        for (var n in objs) {
            if (!objs.hasOwnProperty(n)) { continue; }
            var names2 = [], pp = objs[n].reflect.properties;
            for (var q = 0; q < pp.length; q++) {
                var pn = pp[q].name;
                if (pn === 'parent' || pn === 'reflect' || pn === 'properties' || pn === 'isValid' || pn === 'events' || pn === 'eventListeners') { continue; }
                names2.push(pn + '=' + tryRead(objs[n], pn));
            }
            out.props[n] = names2;
        }
        var d = lib.track(app.documents.add(false));
        var pr = d.printPreferences, names3 = [], rp = pr.reflect.properties;
        for (var r = 0; r < rp.length; r++) {
            var rn = rp[r].name;
            if (rn === 'parent' || rn === 'reflect' || rn === 'properties' || rn === 'isValid' || rn === 'events' || rn === 'eventListeners') { continue; }
            names3.push(rn + '=' + tryRead(pr, rn));
        }
        out.props.documentPrintPreferences = names3;
        out.props.documentTransparency = ['blendingSpace=' + tryRead(d.transparencyPreferences, 'blendingSpace')];
        lib.writeText(lib.args.out, lib.toJSON(out));
    });
    return log;
})();
