// ise-faithful-outlines.jsx - Conservative, staged outline conversion.
// Pure ASCII. Public interfaces: $.global.ISE_FAITHFUL.canConvert/convertFrame.
// Host 21.5.1.73 returned outlines outside the staging group; rollback was clean.
// Outline regrouping reached stroke duplication in host probe 2; its combined
// geometry/parent check failed cleanly. Stroke-copy adoption is source-only tested.
// Scope: unthreaded, non-overset, untransformed frames on normal page/spread parents,
// uniform solid text paint, no frame paint, wrapping, decorations, or effects.
// Unreadable support properties refuse conversion. This is not a general flattener.
// Adobe DOM: https://developer.adobe.com/indesign/uxp/dom/api/g/groups/
// https://developer.adobe.com/indesign/uxp/dom/api/p/polygon/ (move excludes Group).

$.global.ISE_FAITHFUL = (function () {
    var F = {};
    var KEYS = ['strokeAlignment', 'strokeWeight', 'endJoin', 'miterLimit', 'strokeTint', 'overprintStroke', 'fillTint', 'overprintFill'];
    var DECORATIONS = ['underline', 'strikeThru', 'ruleAbove', 'ruleBelow', 'paragraphShadingOn', 'paragraphBorderOn'];
    var EFFECTS = ['innerShadowSettings', 'outerGlowSettings', 'innerGlowSettings', 'bevelAndEmbossSettings', 'satinSettings', 'directionalFeatherSettings', 'gradientFeatherSettings'];

    function need(ok, message) { if (!ok) { throw Error('faithful: ' + message); } }
    function kind(x) { return x && x.constructor ? x.constructor.name : ''; }
    function same(a, b) { return a && b && a.id !== undefined && a.id === b.id; }
    function number(x) { return typeof x === 'number' && isFinite(x); }
    function none(x) { return x && x.isValid && x.name === 'None'; }
    function color(x) { return x && x.isValid && (none(x) || kind(x) === 'Color'); }
    function unlinked(x) { return x === null || x === NothingEnum.NOTHING; }
    function signature(t) {
        var a = [];
        for (var i = 0; i < KEYS.length; i++) { a.push(String(t[KEYS[i]])); }
        a.push(t.strokeColor.id, t.fillColor.id);
        return a.join('|');
    }
    function plainEffects(settings) {
        var b = settings.blendingSettings;
        need(b.blendMode === BlendMode.NORMAL && b.opacity === 100 && b.knockoutGroup === false && b.isolateBlending === false, 'unsupported blending');
        need(settings.dropShadowSettings.mode === ShadowMode.NONE && settings.featherSettings.mode === FeatherMode.NONE, 'unsupported shadow/feather');
        for (var i = 0; i < EFFECTS.length; i++) { need(settings[EFFECTS[i]].applied === false, 'unsupported ' + EFFECTS[i]); }
    }
    function plainItem(item) {
        plainEffects(item.transparencySettings);
        plainEffects(item.fillTransparencySettings);
        plainEffects(item.strokeTransparencySettings);
        plainEffects(item.contentTransparencySettings);
        need(item.textWrapPreferences.textWrapMode === TextWrapModes.NONE, 'text wrap unsupported');
        need(item.nonprinting === false, 'nonprinting art unsupported');
    }
    function preflight(tf) {
        need(kind(tf) === 'TextFrame' && tf.isValid, 'valid text frame required');
        need(kind(tf.parent) === 'Spread' && kind(tf.parentPage) === 'Page' && tf.parentPage.isValid && same(tf.parentPage.parent, tf.parent), 'normal page/spread parent required');
        need(tf.locked === false && tf.itemLayer.locked === false && tf.itemLayer.visible === true, 'locked/hidden art unsupported');
        need(unlinked(tf.previousTextFrame) && unlinked(tf.nextTextFrame), 'threaded text unsupported');
        var story = tf.parentStory;
        need(story.textContainers.length === 1 && same(story.textContainers[0], tf), 'single frame story required');
        need(tf.overflows === false && typeof story.contents === 'string' && /\S/.test(story.contents), 'empty/overset text unsupported');
        need(tf.allPageItems.length === 0 && story.tables.length === 0 && story.footnotes.length === 0, 'anchored art/tables/footnotes unsupported');
        need(none(tf.fillColor) && none(tf.strokeColor), 'frame background/stroke unsupported');
        need(tf.absoluteRotationAngle === 0 && tf.absoluteShearAngle === 0 && tf.horizontalScale === 100 && tf.verticalScale === 100, 'transformed frame unsupported');
        plainItem(tf);
        var ranges = story.textStyleRanges, src = null, sig = null;
        for (var i = 0; i < ranges.length; i++) {
            var t = ranges[i];
            // Blank ranges can carry paragraph shading or rules: inspect them too.
            for (var d = 0; d < DECORATIONS.length; d++) { need(t[DECORATIONS[d]] === false, 'text decoration/background unsupported'); }
            need(t.bulletsAndNumberingListType === ListType.NO_LIST, 'automatic lists unsupported');
            if (!/\S/.test(String(t.contents))) { continue; }
            need(color(t.strokeColor) && color(t.fillColor), 'only solid text colors supported');
            need(number(t.strokeWeight) && t.strokeWeight >= 0 && number(t.miterLimit) && t.miterLimit >= 1, 'invalid stroke metrics');
            need(number(t.strokeTint) && t.strokeTint >= -1 && t.strokeTint <= 100 && number(t.fillTint) && t.fillTint >= -1 && t.fillTint <= 100, 'invalid tint');
            need(typeof t.overprintStroke === 'boolean' && typeof t.overprintFill === 'boolean', 'unknown overprint');
            need(t.strokeAlignment === TextStrokeAlign.OUTSIDE_ALIGNMENT || t.strokeAlignment === TextStrokeAlign.CENTER_ALIGNMENT, 'unsupported stroke alignment');
            need(t.endJoin === EndJoin.MITER_END_JOIN || t.endJoin === EndJoin.ROUND_END_JOIN || t.endJoin === EndJoin.BEVEL_END_JOIN, 'unsupported join');
            var s = signature(t);
            need(sig === null || sig === s, 'mixed stroke/fill settings');
            if (src === null) { src = t; sig = s; }
        }
        need(src !== null, 'no painted text');
        return src;
    }
    function children(item) { return item.pageItems.everyItem().getElements(); }
    function inside(item, root) {
        var p = item.parent;
        while (kind(p) === 'Group') { if (same(p, root)) { return true; } p = p.parent; }
        return false;
    }
    function leaves(item, root, out) {
        need(item.isValid && inside(item, root), 'outline escaped staging group');
        if (kind(item) === 'Group') {
            plainItem(item);
            var kids = children(item);
            item.ungroup();
            for (var k = 0; k < kids.length; k++) { leaves(kids[k], root, out); }
        } else {
            need(kind(item) === 'Polygon', 'non-polygon outline unsupported');
            out.push(item);
        }
    }
    function geometry(item) {
        var paths = item.paths, a = [];
        need(paths.length > 0, 'empty outline paths');
        function walk(v) {
            if (typeof v === 'number') { need(number(v), 'invalid outline coordinate'); a.push(String(v)); }
            else {
                need(v && typeof v.length === 'number' && v.length > 0, 'invalid outline geometry');
                a.push('['); for (var j = 0; j < v.length; j++) { walk(v[j]); } a.push(']');
            }
        }
        for (var i = 0; i < paths.length; i++) { walk(paths[i].entirePath); }
        return a.join(',');
    }
    function assign(item, key, value) {
        try {
            if (item[key] === value) { return; }
            item[key] = value;
            need(item[key] === value, 'could not preserve ' + key);
        } catch (e) { throw Error('faithful: assignment ' + key + ': ' + e.message); }
    }
    function assignColor(item, key, value) {
        item[key] = value;
        need(same(item[key], value), 'could not preserve ' + key);
    }
    F.canConvert = function (tf) {
        try { preflight(tf); return true; } catch (e) { return false; }
    };
    F.convertFrame = function (doc, tf, log) {
        var src;
        try {
            src = preflight(tf);
            need(same(tf.parent.parent, doc), 'frame belongs to another document');
        } catch (unsupported) {
            if (log) { log(String(unsupported) + '; not converted'); }
            return null;
        }
        var a = {}, i;
        for (i = 0; i < KEYS.length; i++) { a[KEYS[i]] = src[KEYS[i]]; }
        a.strokeColor = src.strokeColor; a.fillColor = src.fillColor;
        var outside = a.strokeAlignment === TextStrokeAlign.OUTSIDE_ALIGNMENT;
        var painted = outside ? 2 * a.strokeWeight : a.strokeWeight;
        var hasStroke = a.strokeWeight > 0 && !none(a.strokeColor);
        var name = tf.name, page = tf.parentPage, parent = tf.parent, layer = tf.itemLayer;
        var text = tf.parentStory.contents, sig = signature(src), bounds = String(tf.geometricBounds);
        var noColor = doc.swatches.itemByName('None');
        need(none(noColor), 'None swatch unavailable');
        function overprint(item, key, value, paint) {
            // Paper has knockout semantics; None contributes no ink. In the measured
            // host both expose inapplicable page-item overprint properties. Accept
            // only the source's false state here; never swallow arbitrary write failures.
            if (none(paint) || same(paint, doc.swatches.itemByName('Paper'))) {
                need(value === false, 'overprinting Paper/None is unsupported');
                return;
            }
            assign(item, key, value);
        }
        var stage = null, guard = null, root = null, owned = [], ownedIDs = {}, ready = false, pendingGroup = null;
        function inventory() {
            var ids = {}, items = doc.allPageItems;
            for (var n = 0; n < items.length; n++) { ids['id' + items[n].id] = true; }
            return ids;
        }
        var initialIDs = inventory();
        function remember(x) {
            if (!ownedIDs['id' + x.id]) { owned.push(x); ownedIDs['id' + x.id] = true; }
            return x;
        }
        function groupOwned(items) {
            for (var n = 0; n < items.length; n++) {
                need(ownedIDs['id' + items[n].id] && same(items[n].parent, parent), 'group requires owned spread siblings');
            }
            pendingGroup = {items: items, before: inventory()};
            var added = page.groups.add(items, layer);
            need(added.isValid && !pendingGroup.before['id' + added.id], 'group did not return new art');
            remember(added);
            pendingGroup = null;
            need(same(added.parent, parent) && same(added.parentPage, page) && same(added.itemLayer, layer), 'new group placement changed');
            plainItem(added);
            return added;
        }
        function trackOutput(item, existing, placements) {
            need(item.isValid && item.id !== undefined && !existing['id' + item.id], 'outline returned preexisting/invalid art');
            // Inspect descendants before owning a group: a new container ID alone
            // does not make a preexisting child disposable.
            if (kind(item) === 'Group') {
                var kids = children(item);
                for (var n = 0; n < kids.length; n++) { trackOutput(kids[n], existing, placements); }
                remember(item);
            } else {
                remember(item);
                need(kind(item) === 'Polygon', 'non-polygon outline unsupported');
                placements.push({item: item, bounds: String(item.geometricBounds), shape: geometry(item)});
            }
            plainItem(item);
            need(same(item.parentPage, page) && same(item.itemLayer, layer), 'outline changed page/layer');
        }
        function duplicate() {
            var x = tf.duplicate(undefined, [0, 0]);
            need(x && x.isValid && !same(x, tf), 'invalid duplicate');
            remember(x);
            need(!same(x.parentStory, tf.parentStory) && x.parentStory.contents === text, 'duplicate story not independent');
            need(same(x.parent, parent) && same(x.parentPage, page) && same(x.itemLayer, layer) && String(x.geometricBounds) === bounds, 'duplicate placement changed');
            return x;
        }
        try {
            stage = duplicate(); guard = duplicate();
            // Two items are needed to create a group. The second owned duplicate is
            // a temporary guard; createOutlines is allowed to return spread siblings.
            root = groupOwned([stage, guard]);
            need(same(root.parent, parent) && same(root.parentPage, page) && same(root.itemLayer, layer), 'staging group placement changed');
            plainItem(root);
            need(inside(stage, root) && inside(guard, root) && String(stage.geometricBounds) === bounds, 'staging changed text placement');
            var existing = inventory();
            var result = stage.createOutlines(false), polys = [];
            need(result && result.length > 0, 'empty outline result');
            var placements = [], outputError = null, regroup = [root];
            for (i = 0; i < result.length; i++) {
                try { trackOutput(result[i], existing, placements); }
                catch (invalidOutput) { outputError = invalidOutput; }
            }
            if (outputError) { throw outputError; }
            for (i = 0; i < result.length; i++) {
                if (!inside(result[i], root)) {
                    need(same(result[i].parent, parent), 'unsupported outline parent: ' + kind(result[i].parent) + ' id=' + result[i].parent.id + ', staging id=' + root.id);
                    regroup.push(result[i]);
                }
            }
            if (regroup.length > 1) {
                var oldRoot = root;
                root = groupOwned(regroup);
                need(inside(oldRoot, root), 'regroup did not adopt staging root');
                oldRoot.ungroup(); // Only the owned guard/stage/output subtree.
            }
            for (i = 0; i < result.length; i++) { leaves(result[i], root, polys); }
            for (i = 0; i < placements.length; i++) {
                need(String(placements[i].item.geometricBounds) === placements[i].bounds && geometry(placements[i].item) === placements[i].shape, 'regroup/ungroup changed outline geometry');
            }
            need(polys.length > 0, 'no outline polygons');
            for (i = 0; i < polys.length; i++) { remember(polys[i]); }
            var strokes = [], fills = [], siblingStrokes = [], copyChecks = [];
            for (var p = 0; p < polys.length; p++) {
                var P = polys[p], shape = geometry(P);
                plainItem(P);
                if (hasStroke) {
                    var beforeCopy = inventory(), S = P.duplicate(undefined, [0, 0]);
                    var copyValid = !!(S && S.isValid), copyFresh = !!(copyValid && S.id !== undefined && !beforeCopy['id' + S.id]);
                    need(copyValid && copyFresh, 'stroke copy ownership: valid=' + copyValid + ' fresh=' + copyFresh);
                    remember(S); // Only a verified new returned handle is disposable.
                    need(kind(S) === 'Polygon', 'stroke copy type: polygon=false actual=' + kind(S));
                    var copyInside = inside(S, root), copySibling = same(S.parent, parent);
                    var copyPage = same(S.parentPage, page), copyLayer = same(S.itemLayer, layer);
                    var copyBounds = String(S.geometricBounds) === String(P.geometricBounds);
                    var copyGeometry = false, geometryError = '';
                    try { copyGeometry = geometry(S) === shape; } catch (badShape) { geometryError = String(badShape); }
                    var diagnostic = 'copy id=' + S.id + ' valid=' + copyValid + ' fresh=' + copyFresh +
                        ' contained=' + copyInside + ' sameSpread=' + copySibling + ' samePage=' + copyPage +
                        ' sameLayer=' + copyLayer + ' boundsEqual=' + copyBounds + ' geometryEqual=' + copyGeometry +
                        ' parent=' + kind(S.parent) + ':' + S.parent.id + ' root=' + root.id;
                    need(copyPage && copyLayer, 'stroke copy page/layer: ' + diagnostic);
                    need(copyBounds && copyGeometry, 'stroke copy geometry: ' + diagnostic + (geometryError ? ' ' + geometryError : ''));
                    need(copyInside || copySibling, 'stroke copy unsupported parent: ' + diagnostic);
                    plainItem(S);
                    copyChecks.push(diagnostic);
                    placements.push({item: S, bounds: String(S.geometricBounds), shape: shape});
                    if (!copyInside) { siblingStrokes.push(S); }
                    assignColor(S, 'fillColor', noColor);
                    assignColor(S, 'strokeColor', a.strokeColor);
                    assign(S, 'strokeTint', a.strokeTint);
                    assign(S, 'strokeWeight', painted);
                    assign(S, 'strokeAlignment', StrokeAlignment.CENTER_ALIGNMENT);
                    assign(S, 'endJoin', a.endJoin); assign(S, 'miterLimit', a.miterLimit);
                    overprint(S, 'overprintStroke', a.overprintStroke, a.strokeColor);
                    S.name = 'stroke'; strokes.push(S);
                }
                // Do not set weight=0 after None: the host can reactivate a black stroke.
                assignColor(P, 'strokeColor', noColor);
                assignColor(P, 'fillColor', a.fillColor);
                assign(P, 'fillTint', a.fillTint);
                overprint(P, 'overprintFill', a.overprintFill, a.fillColor);
                need(geometry(P) === shape, 'fill geometry changed');
                P.name = 'fill'; fills.push(P);
            }
            // duplicate(undefined, [0,0]) need not preserve group membership.
            // Batch verified sibling copies with the owned root, then remove only
            // that old owned wrapper. groupOwned retains pending-call recovery.
            if (siblingStrokes.length) {
                var strokeRoot = root;
                root = groupOwned([strokeRoot].concat(siblingStrokes));
                need(inside(strokeRoot, root), 'stroke regroup did not adopt staging root');
                strokeRoot.ungroup();
            }
            for (i = 0; i < placements.length; i++) {
                var placed = placements[i], placedInside = inside(placed.item, root);
                var boundsEqual = String(placed.item.geometricBounds) === placed.bounds;
                var geometryEqual = geometry(placed.item) === placed.shape;
                var samePage = same(placed.item.parentPage, page), sameLayer = same(placed.item.itemLayer, layer);
                need(placedInside && boundsEqual && geometryEqual && samePage && sameLayer,
                    'stroke regroup validation: id=' + placed.item.id + ' contained=' + placedInside +
                    ' boundsEqual=' + boundsEqual + ' geometryEqual=' + geometryEqual + ' samePage=' + samePage + ' sameLayer=' + sameLayer);
            }
            var front = outside ? fills : strokes;
            for (i = 0; i < front.length; i++) { front[i].bringToFront(); }
            stage.remove();
            guard.remove();
            need(root.isValid, 'host dissolved staging group');
            root.bringToFront(tf); // Same spread parent, immediately above source.
            assign(root, 'name', name + ' (faithful outlines)');
            var finalItems = children(root);
            need(finalItems.length === fills.length + strokes.length, 'unexpected staged art remains');
            for (i = 0; i < finalItems.length; i++) {
                need(kind(finalItems[i]) === 'Polygon' && same(finalItems[i].itemLayer, layer), 'invalid final art');
                geometry(finalItems[i]);
                need(finalItems[i].name === (i < front.length ? (outside ? 'fill' : 'stroke') : (outside ? 'stroke' : 'fill')), 'paint stacking did not take effect');
            }
            need(same(root.parent, parent) && same(root.parentPage, page), 'final parent/page changed');
            // pageItems and index can be type-ordered: Group and TextFrame both
            // reported index 0 in the host. allPageItems supplies the observed
            // flattened paint order; compare only direct siblings on this layer.
            var siblings = page.allPageItems, previous = null, adjacent = false;
            for (i = 0; i < siblings.length; i++) {
                if (!same(siblings[i].parent, parent) || !same(siblings[i].itemLayer, layer)) { continue; }
                if (same(siblings[i], tf)) { adjacent = same(previous, root); break; }
                previous = siblings[i];
            }
            need(adjacent, 'replacement did not take source stacking position');
            // The supplied logger is fallible too: call it before destructive commit.
            if (log) { log('faithful: prepared ' + name + ' painted width=' + painted + ' paths=' + polys.length +
                ' adoptedStrokeSiblings=' + siblingStrokes.length + ' ' + copyChecks.join('; ')); }
            need(tf.isValid && tf.parentStory.contents === text && signature(preflight(tf)) === sig && String(tf.geometricBounds) === bounds, 'source changed during staging');
            ready = true;
            tf.remove(); // Commit: retain replacement if deletion succeeds then throws.
            need(!tf.isValid, 'source removal did not complete');
            return root;
        } catch (error) {
            // If the host deletes and then throws, retain the validated replacement.
            if (ready && !tf.isValid) { throw Error(String(error) + '; source removed, validated replacement retained'); }
            // Both initial grouping and sibling regrouping may reparent and then
            // throw before returning. Recover a fresh parent only when every child
            // belongs to that exact pending call. Never sweep new document art.
            var cleanup = [];
            try {
                if (pendingGroup) {
                    var allowed = {}, members = pendingGroup.items;
                    for (i = 0; i < members.length; i++) { allowed['id' + members[i].id] = true; }
                    for (i = 0; i < members.length; i++) {
                        if (!members[i].isValid) { continue; }
                        var candidate = members[i].parent;
                        if (kind(candidate) !== 'Group' || pendingGroup.before['id' + candidate.id]) { continue; }
                        var kids = children(candidate), ours = kids.length > 0;
                        for (var k = 0; k < kids.length; k++) { if (!allowed['id' + kids[k].id]) { ours = false; } }
                        if (ours) { remember(candidate); }
                    }
                }
            } catch (recoveryError) { cleanup.push('group recovery: ' + String(recoveryError)); }
            for (i = owned.length - 1; i >= 0; i--) {
                try {
                    if (owned[i].isValid) {
                        owned[i].remove();
                        need(!owned[i].isValid, 'owned art removal did not complete');
                    }
                }
                catch (failure) { cleanup.push(String(failure)); }
            }
            // A throwing createOutlines call may create external art without returning
            // handles. A document delta cannot establish ownership (e.g. event art).
            // Preserve it, report its IDs, and never claim a clean rollback in that case.
            try {
                var remaining = doc.allPageItems, unclaimed = [];
                for (i = 0; i < remaining.length; i++) {
                    if (!initialIDs['id' + remaining[i].id]) { unclaimed.push(remaining[i].id); }
                }
                if (unclaimed.length) { cleanup.push('remaining new art IDs: ' + unclaimed.join(',')); }
            } catch (auditError) { cleanup.push('could not verify cleanup: ' + String(auditError)); }
            throw Error(String(error) + ' at line ' + error.line + (cleanup.length ? '; rollback incomplete: ' + cleanup.join('; ') : '; staged art rolled back'));
        }
    };
    return F;
})();
