// Read-only native identity/process telemetry. No document creation, mutation, or shutdown.
(function(){
    var raw=app.sendScriptMessage('StrokeMasterProposedProbe','diagnostics','');
    if(!/^\{"schema":2,"plugin":"StrokeMasterProposedProbe","build":"native-prep-4","processId":\d+,/.test(raw)){
        throw Error('Unexpected native diagnostic identity.');
    }
    return '{"passed":true,"scope":"read-only telemetry; no rendering claim","openDocuments":'+app.documents.length+',"native":'+raw+'}';
})();
