// Route re-fix test, phase 2 of 3: the user's recolor, in its own script so the re-fix timer
// can run before phase 3. See ai-route-refix-start.jsx.
(function () {
    var args = $.global.ISE_REGION_ARGS || {};
    if (app.documents.length !== 1) return 'FAIL|expected the phase 1 document';
    var lib2 = $.evalFile(new File(new File($.fileName).parent.fsName + '/ai-route-refix-lib.jsx'));
    try { return 'DONE|recolored=' + lib2.recolor(app.activeDocument); }
    catch (e) { return 'FAIL|' + String(e); }
})();
