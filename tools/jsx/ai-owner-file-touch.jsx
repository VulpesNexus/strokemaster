// A later phase on the document the first phase opened: every text frame and path at the top of
// a layer or in a plain group moved by 1 pt and back, as a hand's edit, so automatic correction
// takes them as edited. One undo step, as a hand's edit would be.
(function () {
    if (app.documents.length !== 1) return 'FAIL|expected the first phase document';
    var doc = app.activeDocument, n = 0;
    function visit(item) {
        if (item.locked || item.hidden) return;
        if (item.typename === 'GroupItem') { for (var i = 0; i < item.pageItems.length; i++) visit(item.pageItems[i]); return; }
        if (item.typename === 'TextFrame' || item.typename === 'PathItem' || item.typename === 'CompoundPathItem') {
            item.translate(1, 0); item.translate(-1, 0); n++;
        }
    }
    for (var L = 0; L < doc.layers.length; L++)
        for (var i = 0; i < doc.layers[L].pageItems.length; i++) visit(doc.layers[L].pageItems[i]);
    doc.selection = null;
    return 'DONE|touched ' + n;
})();
