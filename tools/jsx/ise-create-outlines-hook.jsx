// ise-create-outlines-hook.jsx - route Type > Create Outlines through the faithful converter.
// Pure ASCII. Defines $.global.ISE_HOOK; the caller deletes it afterward.
//
// SUPERSEDED. The faithful conversion is now a command in strokemaster.jsx, which asks for it
// explicitly instead of intercepting a menu. That avoids the two costs of this hook: it has to
// be installed as a startup script to be always on, and while it is on it changes what a
// standard InDesign menu item does, for every document, whether or not that was wanted. This
// file is kept because the interception itself is sound and was verified against the real menu
// command; use it only if silently correcting Type > Create Outlines is the goal.
//
// install(F, log) adds a beforeInvoke listener to the Create Outlines menu action. When the
// selection consists only of text frames that the faithful converter can handle, the listener
// cancels the native command and converts those frames instead. Any other selection (text
// ranges, mixed selections, frames with mixed stroke settings) is left to InDesign.
// uninstall() removes the listener. Function listeners need a persistent script engine, so
// run the caller with #targetengine.

$.global.ISE_HOOK = (function () {
    var H = { action: null, listener: null, handled: 0, passed: 0 };

    function findAction() {
        var byId = app.menuActions.itemByName('$ID/Create Outlines');
        if (byId.isValid) { return byId; }
        var all = app.menuActions.everyItem().getElements();
        for (var i = 0; i < all.length; i++) {
            if (all[i].name === 'Create Outlines') { return all[i]; }
        }
        return null;
    }

    H.install = function (F, log) {
        H.action = findAction();
        if (!H.action) { throw new Error('Create Outlines menu action not found'); }
        H.listener = H.action.addEventListener('beforeInvoke', function (ev) {
            var sel = app.selection, frames = [];
            for (var i = 0; i < sel.length; i++) {
                if (F.canConvert(sel[i])) { frames.push(sel[i]); }
            }
            if (frames.length === 0 || frames.length !== sel.length) {
                H.passed++;
                log('hook: selection left to InDesign (' + sel.length + ' items, ' + frames.length + ' convertible)');
                return;
            }
            ev.preventDefault();
            ev.stopPropagation();
            var doc = app.activeDocument;
            for (var k = 0; k < frames.length; k++) {
                if (F.convertFrame(doc, frames[k], log)) { H.handled++; }
            }
        });
        return H.action.name;
    };

    H.uninstall = function () {
        if (H.action && H.listener) {
            try { H.listener.remove(); } catch (e) {
                try { H.action.removeEventListener('beforeInvoke', H.listener); } catch (e2) {}
            }
        }
        H.listener = null;
    };

    return H;
})();
