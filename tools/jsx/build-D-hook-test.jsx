// build-D-hook-test.jsx - invoke the real Type > Create Outlines menu action with the faithful
// hook installed, and again with it removed, on a throwaway document with a window.
// Requires ise-lib.jsx, ise-faithful-outlines.jsx, and ise-create-outlines-hook.jsx, and a
// persistent engine (run-jsx.ps1 -Engine). The hook is removed before this script returns,
// whatever happens, and the menu action is only invoked while the test document is active.
(function () {
    var lib = $.global.ISE_LIB, F = $.global.ISE_FAITHFUL, H = $.global.ISE_HOOK;
    return lib.run(function () {
        var dir = lib.args.outRoot + '/D-create-outlines';
        var doc = lib.newDoc(612, 230);
        doc.windows.add();
        var base = lib.baseStyle(doc, 'ISE DH base', { pointSize: 48, strokeWeight: 4, endJoin: EndJoin.ROUND_END_JOIN });

        var live1 = lib.frame(doc, 'H01 live text', [20, 24, 90, 290], 'Hog', base);
        lib.record(live1, 'H01: live text; copy converted through the menu with the hook installed', { role: 'text', measure: 'pair' });
        var hooked = live1.duplicate(undefined, [290, 0]);
        hooked.name = 'H01 copy for hooked Create Outlines';

        var live2 = lib.frame(doc, 'H02 live text', [120, 24, 190, 290], 'Hog', base);
        lib.record(live2, 'H02: live text; copy converted through the menu with the hook removed', { role: 'text', measure: 'pair' });
        var plain = live2.duplicate(undefined, [290, 0]);
        plain.name = 'H02 copy for native Create Outlines';

        function assertActive() {
            if (app.activeDocument.id !== doc.id) { throw new Error('test document is not the active document; not invoking the menu action'); }
        }

        try {
            lib.L('hook installed on menu action: ' + H.install(F, lib.L));
            assertActive();
            app.select(hooked);
            try {
                H.action.invoke();
            } catch (cancel) {
                // The hook cancels the native command; a scripted invoke reports that as an error.
                lib.L('hooked invoke raised "' + cancel.message + '" (expected when the hook cancels the native command)');
            }
            lib.L('hooked invoke: handled=' + H.handled + ' passed=' + H.passed);
        } finally {
            H.uninstall();
            lib.L('hook removed');
        }

        assertActive();
        app.select(plain);
        H.action.invoke();
        lib.L('native invoke done');

        function safe(it, key) {
            try { return lib.str(it[key]); } catch (e) { return '(mixed)'; }
        }
        var items = doc.pages[0].allPageItems;
        for (var i = 0; i < items.length; i++) {
            var it = items[i];
            if (it.parent.constructor.name === 'Spread' || it.parent.constructor.name === 'Page') {
                var entry = 'top-level item: ' + it.constructor.name + ' "' + it.name + '"' + (it.constructor.name === 'Group' ? ' children=' + it.pageItems.length : '') +
                    ' strokeAlignment=' + safe(it, 'strokeAlignment') + ' strokeWeight=' + safe(it, 'strokeWeight');
                lib.L(entry);
                if (it.constructor.name !== 'TextFrame' && it.name.indexOf('backdrop') !== 0) {
                    lib.record(it, it.constructor.name === 'Group' ? 'H01: menu Create Outlines with the hook' : 'H02: menu Create Outlines without the hook', { role: 'outlines', cls: it.constructor.name });
                }
            }
        }
        lib.saveExport(doc, dir, 'D-hook', 300);
    });
})();
