// build-A-minimal.jsx - minimal reproducers for adjacent-line stroke overlap (fixture A).
// Requires ise-lib.jsx to be loaded first. Argument: ISE_ARGS.outRoot (tests/indesign-stroke).
// Varies one dimension per frame: line-break kind, alignment, leading, weight, threading,
// z-order against story order, runs within a line, and frame-level text transparency.
(function () {
    var lib = $.global.ISE_LIB;
    return lib.run(function () {
        var dir = lib.args.outRoot + '/A-adjacent-line-overlap';
        var doc = lib.newDoc(612, 792);
        var base = lib.baseStyle(doc, 'ISE A base', { strokeWeight: 8 });
        var magenta = lib.color(doc, 'ISE Magenta', [0, 100, 0, 0]);
        var TWO = 'gyp jq\rHkbd lh';
        var f;

        f = lib.frame(doc, 'A01 two paragraphs, stroke 8 outside', [30, 36, 120, 306], TWO, base);
        lib.record(f, 'two paragraphs; descenders above ascenders', { measure: 'hidden' });

        f = lib.frame(doc, 'A02 forced line break', [30, 318, 120, 588], 'gyp jq\nHkbd lh', base);
        lib.record(f, 'one paragraph, forced line break', { measure: 'hidden' });

        f = lib.frame(doc, 'A03 soft wrap', [140, 36, 230, 136], 'gyp jq Hkbd lh', base);
        lib.record(f, 'one paragraph wrapped by frame width', { measure: 'hidden' });

        f = lib.frame(doc, 'A04 center alignment, stroke 16', [140, 318, 230, 588], TWO, base);
        f.parentStory.texts[0].strokeAlignment = TextStrokeAlign.CENTER_ALIGNMENT;
        f.parentStory.texts[0].strokeWeight = 16;
        lib.record(f, 'Center alignment at twice the weight (same outer extent)', { measure: 'hidden' });

        f = lib.frame(doc, 'A05 control, leading 60', [250, 36, 380, 306], TWO, base);
        f.parentStory.texts[0].leading = 60;
        lib.record(f, 'control: lines far apart, no overlap expected', { measure: 'hidden' });

        f = lib.frame(doc, 'A09a stroke 3', [250, 318, 340, 588], TWO, base);
        f.parentStory.texts[0].strokeWeight = 3;
        lib.record(f, 'weight sweep: 3 pt', { measure: 'hidden' });

        f = lib.frame(doc, 'A09b stroke 5', [360, 318, 450, 588], TWO, base);
        f.parentStory.texts[0].strokeWeight = 5;
        lib.record(f, 'weight sweep: 5 pt', { measure: 'hidden' });

        // Unthreaded pair: lower frame created first, upper frame on top in z-order.
        var lower = lib.frame(doc, 'A08 lower (created first)', [430, 36, 462, 306], 'Hkbd lh', base);
        var upper = lib.frame(doc, 'A08 upper (created second, in front)', [401.2, 36, 433.2, 306], 'gyp jq', base);
        lib.record(lower, 'unthreaded pair: lower frame, behind', { group: 'A08', measure: 'hidden' });
        lib.record(upper, 'unthreaded pair: upper frame, in front', { group: 'A08' });

        // Threaded pair: story starts in the lower frame and continues in the upper frame.
        var d6 = lib.frame(doc, 'A06 lower, story start', [540, 36, 572, 306], 'Hkbd lh\rgyp jq', base);
        var u6 = doc.pages[0].textFrames.add({ geometricBounds: [511.2, 36, 543.2, 306] });
        u6.name = 'A06 upper, story continues (in front)';
        u6.textFramePreferences.insetSpacing = [0, 0, 0, 0];
        d6.nextTextFrame = u6;
        lib.record(d6, 'threaded: story line 1 in the lower frame', { group: 'A06', measure: 'hidden' });
        lib.record(u6, 'threaded: story line 2 in the upper frame, upper frame in front', { group: 'A06' });

        // Same threading with the upper frame sent behind the lower frame.
        var d7 = lib.frame(doc, 'A07 lower, story start (in front)', [540, 318, 572, 588], 'Hkbd lh\rgyp jq', base);
        var u7 = doc.pages[0].textFrames.add({ geometricBounds: [511.2, 318, 543.2, 588] });
        u7.name = 'A07 upper, story continues (behind)';
        u7.textFramePreferences.insetSpacing = [0, 0, 0, 0];
        d7.nextTextFrame = u7;
        u7.sendBackward();
        lib.record(d7, 'threaded: story line 1 in the lower frame, lower frame in front', { group: 'A07', measure: 'hidden' });
        lib.record(u7, 'threaded: story line 2 in the upper frame, upper frame behind', { group: 'A07' });

        f = lib.frame(doc, 'A10 two runs on line 1', [620, 36, 710, 306], TWO, base);
        f.parentStory.characters.itemByRange(2, 3).fillColor = magenta;
        lib.record(f, 'line 1 split into three runs by a fill color change', { measure: 'hidden' });

        f = lib.frame(doc, 'A11 frame text opacity 90', [620, 318, 710, 588], TWO, base);
        lib.tryDo('A11 text transparency', function () { f.contentTransparencySettings.blendingSettings.opacity = 90; });
        lib.record(f, 'frame-level text transparency (opacity 90 %)', { measure: 'hidden' });

        lib.saveExport(doc, dir, 'A-minimal', 300);
    });
})();
