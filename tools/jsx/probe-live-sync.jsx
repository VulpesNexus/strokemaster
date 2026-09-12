// probe-live-sync.jsx - can a script keep two frames in sync without a text-change event?
// Requires ise-lib.jsx. Arguments: ISE_ARGS.mode = 'start' | 'read' | 'stop'.
// Must run in a named persistent engine, or the handler is discarded when the script ends:
//   powershell -NoProfile -File tools\run-jsx.ps1 probe-live-sync.jsx -Engine iseLiveSync
//
// The split in ise-split-stroked-text.jsx turns one story into two frames, and InDesign has no
// native way to keep them together while they carry different formatting
// (tools/jsx/probe-style-mapping.jsx). A script could do it instead, but InDesign raises no
// event when text changes: there is no afterTextChanged. The documented way to get periodic
// work done is an idle task, which is also what the Create Outlines hook relies on, so before
// designing a sync around it, this probe establishes three things in the host:
//
//   L1  app.idleTasks exists and a task can be added with a sleep interval.
//   L2  the ON_IDLE handler actually fires, and keeps firing after the script that
//       registered it has returned. A counter in the persistent engine proves it.
//   L3  document events that could serve as a backstop can be listened to, and which ones
//       exist at all.
//
// The task is removed by mode 'stop'. Leaving a timer running in someone's InDesign would be
// rude and would outlive this investigation, so 'start' also removes any task it replaces.
(function () {
    var lib = $.global.ISE_LIB;
    var out = lib.run(function () {
        var mode = String(lib.args.mode || 'read');
        var TASK = 'iseLiveSyncProbe';
        lib.L('mode=' + mode + ' engine=' + ($.engineName || '(unnamed)'));

        // L1: is the machinery present at all?
        lib.tryDo('L1 idleTasks', function () {
            lib.L('L1 app.idleTasks exists=' + (app.idleTasks !== undefined) + ' count=' + app.idleTasks.length);
            return 'ok';
        });

        function removeTask(why) {
            lib.tryDo('remove task', function () {
                var t = app.idleTasks.itemByName(TASK);
                if (t.isValid) {
                    t.sleep = 0;
                    t.remove();
                    lib.L('removed idle task (' + why + ')');
                } else {
                    lib.L('no idle task to remove (' + why + ')');
                }
                return 'ok';
            });
        }

        if (mode === 'start') {
            removeTask('replacing');
            $.global.ISE_TICKS = 0;
            $.global.ISE_LAST = '';
            lib.tryDo('L2 register', function () {
                var t = app.idleTasks.add({ name: TASK, sleep: 500 });
                t.addEventListener(IdleEvent.ON_IDLE, function () {
                    // Deliberately touches no document: a handler that outlives this run must
                    // not reference anything that can be closed underneath it.
                    $.global.ISE_TICKS = ($.global.ISE_TICKS || 0) + 1;
                    $.global.ISE_LAST = new Date().toString();
                });
                lib.L('L2 registered ' + TASK + ' sleep=' + t.sleep + ' ms, valid=' + t.isValid);
                return 'ok';
            });
        } else if (mode === 'stop') {
            lib.L('L2 final ticks=' + lib.str($.global.ISE_TICKS) + ' last=' + lib.str($.global.ISE_LAST));
            removeTask('asked to stop');
            try { delete $.global.ISE_TICKS; } catch (e) {}
            try { delete $.global.ISE_LAST; } catch (e2) {}
        } else {
            var t = app.idleTasks.itemByName(TASK);
            lib.L('L2 task present=' + t.isValid + ' ticks=' + lib.str($.global.ISE_TICKS) + ' last=' + lib.str($.global.ISE_LAST));
        }

        // L3: which document events could act as a backstop, and can one be attached?
        lib.tryDo('L3 event names', function () {
            var names = [];
            for (var k in DocumentEvent) {
                if (/SAVE|CLOSE|SELECT|CONTEXT|IDLE|TEXT|CHANG/i.test(k)) { names.push(k + '=' + DocumentEvent[k]); }
            }
            lib.L('L3 DocumentEvent candidates: ' + (names.length ? names.join(', ') : '(none matched)'));
            return 'ok';
        });
        lib.tryDo('L3 attach beforeSave', function () {
            if (app.documents.length === 0) { lib.L('L3 no document open; skipping attach test'); return 'skipped'; }
            var d = app.documents[0];
            var fn = function () {};
            var added = d.addEventListener(DocumentEvent.BEFORE_SAVE, fn);
            lib.L('L3 beforeSave listener attached=' + lib.str(added !== undefined));
            d.removeEventListener(DocumentEvent.BEFORE_SAVE, fn);
            lib.L('L3 beforeSave listener removed again');
            return 'ok';
        });
        lib.tryDo('L3 is there a text-change event?', function () {
            var hits = [];
            for (var k in DocumentEvent) { if (/TEXT|STORY|CONTENT/i.test(k)) { hits.push(k); } }
            lib.L('L3 text-change-like events: ' + (hits.length ? hits.join(', ') : 'NONE (polling is the only route)'));
            return 'ok';
        });
    });

    // A script redirected into a named persistent engine with #targetengine does not return its
    // completion value to DoScript, so the caller sees nothing whether the run succeeded or
    // threw. Any script that needs an engine therefore has to report through a file instead;
    // reading silence as success is how an earlier probe in this investigation appeared to do
    // nothing at all.
    try {
        lib.writeText(String(lib.args.logPath || (Folder.temp.fsName + '/ise-live-sync.log')), out);
    } catch (e) {}
    return out;
})();
