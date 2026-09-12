// probe-linked-stories.jsx - can two superposed frames share one live text, natively?
// Requires ise-lib.jsx. Argument: ISE_ARGS.outRoot (unused; the probe only logs).
//
// The companion workaround splits stroked text into a fill-only frame over a stroke-only frame.
// That trades seamlessness for compatibility, and the trade is only worth making if the two
// frames stay in sync for a user who has no script installed. InDesign's linked content is the
// candidate. Research says updating a linked story discards the child's local text formatting,
// which would destroy the whole point, so this probe settles the questions in the host rather
// than from documentation:
//
//   Q1  Does contentPlace exist, and can it create a linked story on the SAME page?
//   Q2  What does Link.linkType report, and what are linkedStoryOptions' real properties?
//   Q3  Can the child carry different formatting from the parent at all?
//   Q4  Does editing the parent mark the child out of date, and does update() then WIPE the
//       child's formatting? This is the question the design lives or dies on.
//   Q5  Does a paragraph style survive an update where a local override would not?
//   Q6  Can contentPlace place linked content INTO an existing frame, or only a new one?
//   Q7  Registration: does adding a stroke move any glyph? If it does, superposed frames
//       cannot line up and the workaround is unsound before any of the above matters.
//
// The probe builds its own document and closes it without saving. It never touches a document
// it did not create.
(function () {
    var lib = $.global.ISE_LIB;
    return lib.run(function () {
        var doc = lib.newDoc(612, 500);
        var paper = doc.swatches.itemByName('Paper');
        var black = doc.swatches.itemByName('Black');
        var none = doc.swatches.itemByName('None');

        // The two styles the workaround would use. Stroke settings live in the styles, not as
        // local overrides, because an update is reported to discard overrides.
        var topStyle = doc.paragraphStyles.add({ name: 'ISE top fill only' });
        topStyle.appliedFont = lib.FONT;
        topStyle.pointSize = 24;
        topStyle.fillColor = black;
        topStyle.strokeColor = none;

        var botStyle = doc.paragraphStyles.add({ name: 'ISE bottom stroke only' });
        botStyle.appliedFont = lib.FONT;
        botStyle.pointSize = 24;
        botStyle.fillColor = paper;          // the stroke color, used as the fill
        botStyle.strokeColor = paper;
        botStyle.strokeWeight = 8;
        botStyle.strokeAlignment = TextStrokeAlign.OUTSIDE_ALIGNMENT;
        botStyle.endJoin = EndJoin.ROUND_END_JOIN;

        var parent = lib.frame(doc, 'parent', [40, 40, 120, 400], 'Sync probe one', topStyle);

        // ---- Q7 registration, first, because everything else is moot if glyphs move --------
        var plain = lib.frame(doc, 'reg plain', [300, 40, 380, 400], 'Sync probe one', topStyle);
        var stroked = lib.frame(doc, 'reg stroked', [400, 40, 480, 400], 'Sync probe one', botStyle);
        // Compare horizontal offsets on their own, and the baselines against the frames' own
        // separation. The two frames sit 100 pt apart deliberately, so comparing both
        // coordinates at once reports a difference that is only the frame placement and says
        // nothing about whether the stroke moved a glyph.
        function wordX(tf) {
            var out = [], w = tf.parentStory.words;
            for (var i = 0; i < w.length; i++) {
                out.push(String(w[i].contents) + '@' + w[i].horizontalOffset.toFixed(4));
            }
            return out.join(' ');
        }
        var pX = wordX(plain), sX = wordX(stroked);
        var dBase = stroked.parentStory.words[0].baseline - plain.parentStory.words[0].baseline;
        var dFrame = stroked.geometricBounds[0] - plain.geometricBounds[0];
        var sameX = (pX === sX), sameY = (Math.abs(dBase - dFrame) < 0.0001);
        lib.L('Q7 horizontal plain   : ' + pX);
        lib.L('Q7 horizontal stroked : ' + sX);
        lib.L('Q7 baseline delta=' + dBase.toFixed(4) + ' frame top delta=' + dFrame.toFixed(4) +
              ' (equal means the stroke shifted nothing vertically)');
        lib.L('Q7 RESULT: horizontal ' + (sameX ? 'IDENTICAL' : 'DIFFERS') +
              ', vertical ' + (sameY ? 'IDENTICAL' : 'DIFFERS') +
              '; superposition ' + ((sameX && sameY) ? 'is sound' : 'would misregister'));

        // ---- Q1 create a linked story on the same page -------------------------------------
        var layer = doc.layers[0];
        var placed = lib.tryDo('Q1 contentPlace', function () {
            return doc.pages[0].contentPlace([parent], false, true, false, [40, 160], layer, false);
        });
        // The return value's shape is undocumented, and reading a property it does not have
        // throws. Describing it must not be able to abort the rest of the probe.
        lib.L('Q1 contentPlace returned: ' + lib.str(placed));
        lib.tryDo('Q1 returned length', function () { lib.L('Q1 returned length=' + placed.length); return 'ok'; });

        var child = null;
        lib.tryDo('Q1 find child', function () {
            var items = doc.pages[0].textFrames;
            for (var i = 0; i < items.length; i++) {
                if (items[i].id !== parent.id && String(items[i].contents).indexOf('Sync probe') === 0 &&
                    items[i].id !== plain.id && items[i].id !== stroked.id) {
                    child = items[i];
                }
            }
            return child ? 'found id=' + child.id : 'no child frame found';
        });
        lib.L('Q1 child: ' + (child ? 'created, id=' + child.id + ' contents=' + String(child.contents) : 'NONE'));

        // ---- Q2 what the link reports -------------------------------------------------------
        lib.L('Q2 document links count: ' + doc.links.length);
        lib.tryDo('Q2 link details', function () {
            for (var i = 0; i < doc.links.length; i++) {
                var lk = doc.links[i];
                lib.L('Q2 link[' + i + '] type=' + lib.str(lk.linkType) + ' status=' + lib.str(lk.status) +
                      ' name=' + lib.str(lk.name) + ' edited=' + lib.str(lk.edited));
            }
            return 'listed';
        });
        lib.tryDo('Q2 linkedStoryOptions', function () {
            var o = doc.linkedStoryOptions, props = o.reflect.properties, names = [];
            for (var i = 0; i < props.length; i++) { names.push(props[i].name); }
            lib.L('Q2 linkedStoryOptions properties: ' + names.join(', '));
            lib.L('Q2 updateWhileSaving=' + lib.str(o.updateWhileSaving) + ' warnOnUpdateOfEditedStory=' + lib.str(o.warnOnUpdateOfEditedStory));
            return 'listed';
        });

        // ---- Q3 give the child different formatting -----------------------------------------
        if (child) {
            lib.tryDo('Q3 restyle child', function () {
                child.parentStory.texts[0].applyParagraphStyle(botStyle, true);
                return 'applied';
            });
            lib.L('Q3 child style now: ' + lib.str(child.parentStory.paragraphs[0].appliedParagraphStyle.name) +
                  ' strokeWeight=' + lib.str(child.parentStory.characters[0].strokeWeight) +
                  ' strokeColor=' + lib.str(child.parentStory.characters[0].strokeColor.name) +
                  ' fillColor=' + lib.str(child.parentStory.characters[0].fillColor.name));
        }

        // ---- Q4 and Q5 edit the parent, then update ------------------------------------------
        lib.tryDo('Q4 edit parent', function () {
            parent.parentStory.contents = 'Sync probe two changed';
            return 'edited';
        });
        lib.L('Q4 after edit, parent=' + String(parent.contents) + ' child=' + (child ? String(child.contents) : 'n/a'));
        lib.tryDo('Q4 link status after edit', function () {
            for (var i = 0; i < doc.links.length; i++) {
                lib.L('Q4 link[' + i + '] status=' + lib.str(doc.links[i].status) + ' edited=' + lib.str(doc.links[i].edited));
            }
            return 'listed';
        });

        var oldUIL = app.scriptPreferences.userInteractionLevel;
        app.scriptPreferences.userInteractionLevel = UserInteractionLevels.NEVER_INTERACT;
        lib.tryDo('Q4 update links', function () {
            doc.links.everyItem().update();
            return 'updated';
        });
        app.scriptPreferences.userInteractionLevel = oldUIL;

        if (child) {
            lib.L('Q4 after update, child contents=' + String(child.contents));
            lib.tryDo('Q5 child formatting after update', function () {
                var c = child.parentStory.characters[0];
                lib.L('Q5 child style=' + lib.str(child.parentStory.paragraphs[0].appliedParagraphStyle.name) +
                      ' strokeWeight=' + lib.str(c.strokeWeight) +
                      ' strokeColor=' + lib.str(c.strokeColor.name) +
                      ' fillColor=' + lib.str(c.fillColor.name) +
                      ' alignment=' + lib.str(c.strokeAlignment));
                return 'read';
            });
            lib.L('Q5 VERDICT: text synced=' + (String(child.contents).indexOf('two changed') >= 0) +
                  ' formatting kept=' + (String(child.parentStory.paragraphs[0].appliedParagraphStyle.name) === 'ISE bottom stroke only'));
        }

        // ---- Q6 can linked content go INTO an existing frame? --------------------------------
        var existing = lib.frame(doc, 'existing target', [200, 40, 280, 400], 'placeholder', topStyle);
        lib.tryDo('Q6 contentPlace into existing frame', function () {
            var res = existing.contentPlace([parent], false, true, false);
            lib.L('Q6 returned ' + lib.str(res) + ' target contents now=' + String(existing.contents));
            return 'called';
        });
        lib.L('Q6 links after: ' + doc.links.length);
    });
})();
