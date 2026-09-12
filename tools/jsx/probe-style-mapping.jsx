// probe-style-mapping.jsx - can a linked story keep its own formatting through an update?
// Requires ise-lib.jsx. Argument: ISE_ARGS.outRoot (unused; the probe only logs).
//
// probe-linked-stories.jsx established that a linked story syncs the text and then overwrites
// the child's formatting with the parent's, paragraph style included. That kills the naive
// version of the companion workaround, where a fill-only frame sits over a stroke-only frame
// and both must show the same live text.
//
// One documented mechanism remains: Custom Style Mapping, which is what Alternate Layouts use
// to carry one story at two sets of styles. If an update re-applies the MAPPED style instead of
// the parent's, the bottom frame can keep its stroke settings. This probe answers:
//
//   S1  Is any style-mapping table reachable from script, or is it UI-only?
//   S2  Does contentPlace with mapStyles = true behave differently from mapStyles = false?
//   S3  With applyStyleMappings = true, does the child keep its own paragraph style through an
//       update? This is the question the whole approach rests on.
//   S4  Does mapStyles = true generate style groups, the way Alternate Layouts do?
//   S5  Can the update warning be suppressed, so an end user is not prompted every time?
//
// Two children are built from one parent, one placed with mapStyles off and one with it on, so
// the comparison is a control rather than a single observation. The probe builds its own
// document and closes it without saving.
(function () {
    var lib = $.global.ISE_LIB;
    return lib.run(function () {
        var doc = lib.newDoc(612, 640);
        var paper = doc.swatches.itemByName('Paper');
        var black = doc.swatches.itemByName('Black');
        var none = doc.swatches.itemByName('None');

        var topStyle = doc.paragraphStyles.add({ name: 'ISE top fill only' });
        topStyle.appliedFont = lib.FONT;
        topStyle.pointSize = 24;
        topStyle.fillColor = black;
        topStyle.strokeColor = none;

        var botStyle = doc.paragraphStyles.add({ name: 'ISE bottom stroke only' });
        botStyle.appliedFont = lib.FONT;
        botStyle.pointSize = 24;
        botStyle.fillColor = paper;
        botStyle.strokeColor = paper;
        botStyle.strokeWeight = 8;
        botStyle.strokeAlignment = TextStrokeAlign.OUTSIDE_ALIGNMENT;
        botStyle.endJoin = EndJoin.ROUND_END_JOIN;

        var parent = lib.frame(doc, 'parent', [40, 40, 110, 400], 'Mapping probe one', topStyle);

        // ---- S1 is a mapping table reachable at all? ----------------------------------------
        function surface(obj, label) {
            lib.tryDo('S1 ' + label, function () {
                var p = obj.reflect.properties, m = obj.reflect.methods, hits = [];
                for (var i = 0; i < p.length; i++) { if (/map|mapping/i.test(p[i].name)) { hits.push('property ' + p[i].name); } }
                for (var j = 0; j < m.length; j++) { if (/map|mapping/i.test(m[j].name)) { hits.push('method ' + m[j].name); } }
                lib.L('S1 ' + label + ': ' + (hits.length ? hits.join(', ') : 'nothing matching map'));
                return 'listed';
            });
        }
        surface(app, 'application');
        surface(doc, 'document');
        surface(doc.linkedStoryOptions, 'document.linkedStoryOptions');
        surface(parent.parentStory, 'story');

        var groupsBefore = doc.paragraphStyleGroups.length;

        // ---- S2 two children, mapStyles off and on -------------------------------------------
        function makeChild(y, mapStyles, label) {
            var made = lib.tryDo('S2 place ' + label, function () {
                return doc.pages[0].contentPlace([parent], false, true, mapStyles, [40, y], doc.layers[0], false);
            });
            lib.L('S2 ' + label + ' (mapStyles=' + mapStyles + ') placed: ' + lib.str(made));
            return made;
        }
        var childA = makeChild(150, false, 'child A');
        var childB = makeChild(260, true, 'child B');

        lib.L('S4 paragraph style groups: before=' + groupsBefore + ' after=' + doc.paragraphStyleGroups.length);
        lib.tryDo('S4 group names', function () {
            var names = [];
            for (var i = 0; i < doc.paragraphStyleGroups.length; i++) { names.push(doc.paragraphStyleGroups[i].name); }
            lib.L('S4 group names: ' + (names.length ? names.join(', ') : '(none)'));
            var sn = [];
            for (var j = 0; j < doc.paragraphStyles.length; j++) { sn.push(doc.paragraphStyles[j].name); }
            lib.L('S4 paragraph styles: ' + sn.join(', '));
            return 'listed';
        });

        // ---- S5 suppress the warning, and turn mapping on -------------------------------------
        lib.tryDo('S5 options', function () {
            doc.linkedStoryOptions.warnOnUpdateOfEditedStory = false;
            doc.linkedStoryOptions.applyStyleMappings = true;
            lib.L('S5 warnOnUpdateOfEditedStory=' + doc.linkedStoryOptions.warnOnUpdateOfEditedStory +
                  ' applyStyleMappings=' + doc.linkedStoryOptions.applyStyleMappings +
                  ' updateWhileSaving=' + doc.linkedStoryOptions.updateWhileSaving);
            return 'set';
        });

        // ---- S3 restyle both children, edit the parent, update -------------------------------
        function restyle(tf, label) {
            if (!tf) { return; }
            lib.tryDo('S3 restyle ' + label, function () {
                tf.parentStory.texts[0].applyParagraphStyle(botStyle, true);
                return 'applied';
            });
            lib.L('S3 ' + label + ' before update: style=' + lib.str(tf.parentStory.paragraphs[0].appliedParagraphStyle.name) +
                  ' weight=' + lib.str(tf.parentStory.characters[0].strokeWeight) +
                  ' stroke=' + lib.str(tf.parentStory.characters[0].strokeColor.name) +
                  ' fill=' + lib.str(tf.parentStory.characters[0].fillColor.name));
        }
        restyle(childA, 'child A');
        restyle(childB, 'child B');

        lib.tryDo('S3 edit parent', function () {
            parent.parentStory.contents = 'Mapping probe two changed';
            return 'edited';
        });
        lib.tryDo('S3 update', function () { doc.links.everyItem().update(); return 'updated'; });

        function report(tf, label) {
            if (!tf) { return; }
            lib.tryDo('S3 report ' + label, function () {
                var c = tf.parentStory.characters[0];
                var kept = String(tf.parentStory.paragraphs[0].appliedParagraphStyle.name) === 'ISE bottom stroke only';
                var synced = String(tf.contents).indexOf('two changed') >= 0;
                lib.L('S3 ' + label + ' after update: style=' + lib.str(tf.parentStory.paragraphs[0].appliedParagraphStyle.name) +
                      ' weight=' + lib.str(c.strokeWeight) + ' stroke=' + lib.str(c.strokeColor.name) +
                      ' fill=' + lib.str(c.fillColor.name));
                lib.L('S3 ' + label + ' VERDICT: text synced=' + synced + ' formatting kept=' + kept);
                return 'reported';
            });
        }
        report(childA, 'child A');
        report(childB, 'child B');
    });
})();
