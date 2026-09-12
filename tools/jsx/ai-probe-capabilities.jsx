// Compatibility entry point for the isolated P1/P2/P3 diagnostic probe.
// The replacement executes native Unite and checks measured area; it never mutates user art.
(function () {
    return $.evalFile(new File(File($.fileName).parent.fsName + '/ai-probe-regions.jsx'));
})();
