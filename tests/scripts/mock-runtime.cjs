// Pure Node mocks: execute repository JSX, never connect to or emulate an Adobe renderer.
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const jsx = path.resolve(__dirname, '../../tools/jsx');

function runtime() {
    const calls = { dialogs: 0, alerts: [], files: [], folders: [] };
    const choices = [];
    const tasks = new Map();
    const context = vm.createContext({});
    context.$ = { global: {}, fileName: path.join(jsx, 'strokemaster.jsx'), engineName: 'pure-test' };
    context.File = function (name) {
        return {
            fsName: String(name), exists: true,
            parent: { fsName: path.dirname(String(name)), create() { calls.folders.push(path.dirname(String(name))); } },
            open(mode) { calls.files.push({ name: String(name), mode }); return true; },
            write(text) { calls.files.at(-1).text = text; },
            close() {}
        };
    };
    context.Folder = function (name) { return { create() { calls.folders.push(String(name)); return true; } }; };
    context.UserInteractionLevels = { NEVER_INTERACT: 0 };
    context.SaveOptions = { NO: 'NO' };
    context.ExportFormat = { PDF_TYPE: 'pdf', PNG_FORMAT: 'png', INDESIGN_MARKUP: 'idml' };
    context.PNGQualityEnum = { MAXIMUM: 'maximum' };
    context.PNGColorSpaceEnum = { RGB: 'rgb' };
    context.PNGExportRangeEnum = { EXPORT_ALL: 'all' };
    context.TextStrokeAlign = { OUTSIDE_ALIGNMENT: 'OUTSIDE_ALIGNMENT', CENTER_ALIGNMENT: 'CENTER_ALIGNMENT' };
    context.EndJoin = { ROUND_END_JOIN: 'ROUND_END_JOIN', MITER_END_JOIN: 'MITER_END_JOIN', BEVEL_END_JOIN: 'BEVEL_END_JOIN' };
    context.IdleEvent = { ON_IDLE: 'idle' };
    context.BlendMode = { NORMAL: 'NORMAL' };
    context.ShadowMode = { NONE: 'NONE' };
    context.FeatherMode = { NONE: 'NONE' };
    context.app = {
        version: 'pure-mock', documents: [], selection: [],
        scriptPreferences: { userInteractionLevel: 1 },
        pdfExportPreferences: { reflect: { properties: [] } },
        pngExportPreferences: { reflect: { properties: [] } },
        epsExportPreferences: { reflect: { properties: [] } },
        pdfExportPresets: { itemByName: name => ({ name }) },
        idleTasks: {
            itemByName: name => tasks.get(name) || { isValid: false },
            add({ name, sleep }) {
                const task = {
                    name, sleep, isValid: true, listeners: [],
                    addEventListener(event, callback) { this.listeners.push({ event, callback }); },
                    remove() { this.isValid = false; tasks.delete(name); }
                };
                tasks.set(name, task);
                return task;
            }
        }
    };
    context.Window = function () {
        calls.dialogs++;
        const choice = choices.shift();
        if (choice instanceof Error) { throw choice; }
        if (choice === undefined) { throw new Error('Unexpected UI dialog in pure test'); }
        const radios = [];
        function add(type) {
            const control = { add };
            if (type === 'radiobutton') { radios.push(control); }
            return control;
        }
        return {
            add,
            show() {
                if (context.app.scriptPreferences.userInteractionLevel === 0) { throw new Error('Dialog shown with interaction disabled'); }
                if (choice === null) { return 0; }
                radios.forEach((radio, i) => { radio.value = i === choice; });
                return 1;
            }
        };
    };
    context.alert = text => calls.alerts.push(text);
    function load(name) {
        // The sole removed syntax is an ExtendScript engine directive, not executable code.
        const file = path.join(jsx, name);
        const source = fs.readFileSync(file, 'utf8').replace(/^#targetengine[^\r\n]*/m, '');
        return vm.runInContext(source, context, { filename: file, timeout: 2000 });
    }
    context.$.evalFile = file => load(path.basename(file.fsName));
    return {
        context, calls, choices, tasks, load,
        evaluate: source => vm.runInContext(source, context, { timeout: 2000 }),
        get globals() { return context.$.global; },
        get app() { return context.app; }
    };
}

function document(id = 1, name = 'existing.indd') {
    const labels = {};
    const doc = {
        id, name, saved: true, modified: true, isValid: true,
        constructor: { name: 'Document' }, closeCalls: [],
        close(option) { this.closeCalls.push(option); this.isValid = false; },
        pages: [],
        get allPageItems() { return this.pages.flatMap(page => page.allPageItems); },
        extractLabel: key => labels[key] || '',
        insertLabel(key, value) { labels[key] = value; },
        swatches: { itemByName: name => ({ name, isValid: true, constructor: { name: 'Color' } }) }
    };
    const spread = { id: 100, constructor: { name: 'Spread' }, parent: doc };
    doc.pages.push({ id: 101, constructor: { name: 'Page' }, parent: spread, allPageItems: [] });
    return doc;
}

// A small mutable page-item model. Indices follow front-to-back order. Duplication and
// removal update the same collection read by the actual sync library. Faults are explicit.
function syncFixture({ contents = ['new text', 'old text'], removeFails = false, orderFails = false } = {}) {
    const rt = runtime();
    const doc = document();
    const items = doc.pages[0].allPageItems;
    const layer = { id: 102, locked: false, constructor: { name: 'Layer' } };
    let nextId = 1;
    function plainTransparency() {
        const value = {
            blendingSettings: { blendMode: 'NORMAL', opacity: 100, knockoutGroup: false, isolateBlending: false },
            dropShadowSettings: { mode: 'NONE' }, featherSettings: { mode: 'NONE' }
        };
        for (const effect of ['innerShadowSettings', 'outerGlowSettings', 'innerGlowSettings', 'bevelAndEmbossSettings', 'satinSettings', 'directionalFeatherSettings', 'gradientFeatherSettings']) {
            value[effect] = { applied: false };
        }
        return value;
    }
    function frame(tag, text, pointSize = 24) {
        const paint = {
            strokeColor: { name: tag === 'fill' ? 'None' : 'Paper' },
            strokeTint: 100, strokeWeight: 8, strokeAlignment: 'OUTSIDE_ALIGNMENT',
            endJoin: 'ROUND_END_JOIN', miterLimit: 4, overprintStroke: false, pointSize
        };
        paint.fillColor = { name: tag === 'stroke' ? 'Paper' : 'Black' };
        paint.fillTint = 100;
        paint.overprintFill = false;
        paint.index = 0;
        paint.characters = { length: text.length };
        const item = {
            id: nextId++, name: tag, isValid: true, tag,
            constructor: { name: 'TextFrame' }, geometricBounds: [0, 0, 100, 100],
            parentPage: doc.pages[0], parent: doc.pages[0].parent, itemLayer: layer,
            previousTextFrame: null, nextTextFrame: null, locked: false,
            fillColor: { name: 'None' }, strokeColor: { name: 'None' },
            transparencySettings: plainTransparency(), contentTransparencySettings: plainTransparency(),
            fillTransparencySettings: plainTransparency(), strokeTransparencySettings: plainTransparency(),
            parentStory: { contents: text, characters: { length: text.length, itemByRange: () => paint }, texts: [paint], textStyleRanges: [paint] },
            get index() { return items.indexOf(this); },
            extractLabel() { return this.tag; },
            insertLabel(key, value) { this.tag = value; },
            duplicate() {
                const copy = frame(this.tag, this.parentStory.contents, this.parentStory.texts[0].pointSize);
                Object.assign(copy.parentStory.texts[0], this.parentStory.texts[0]);
                copy.geometricBounds = this.geometricBounds.slice();
                copy.parentPage = this.parentPage;
                copy.parent = this.parent;
                copy.itemLayer = this.itemLayer;
                items.splice(this.index, 0, copy);
                return copy;
            },
            remove() {
                if (removeFails && this === bottom) { throw new Error('Injected locked bottom'); }
                items.splice(this.index, 1);
                this.isValid = false;
            },
            sendBackward() {
                if (orderFails) { return; }
                const i = this.index;
                if (i < items.length - 1) { [items[i], items[i + 1]] = [items[i + 1], items[i]]; }
            }
        };
        return item;
    }
    const top = frame('fill', contents[0]);
    const bottom = frame('stroke', contents[1]);
    items.push(top, bottom);
    function bind(a = top, b = bottom, token = 'pure-pair') {
        a.tag = `ISE3|${token}|fill`;
        b.tag = `ISE3|${token}|stroke`;
    }
    bind();
    rt.app.documents = [doc];
    rt.app.activeDocument = doc;
    rt.load('ise-live-sync.jsx');
    return { rt, api: rt.globals.ISE_SYNC, doc, items, top, bottom, frame, bind };
}

module.exports = { runtime, document, syncFixture };
