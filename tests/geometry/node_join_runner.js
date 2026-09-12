// Execute only the host-independent JSX definitions. No Adobe host objects exist.
const fs = require('fs');
const vm = require('vm');
const source = fs.readFileSync(process.argv[2], 'utf8');
if (/[^\x00-\x7f]/.test(source)) throw new Error('JSX must be ASCII');
const context = { $: { global: {} } };
vm.createContext(context);
vm.runInContext(source, context, { timeout: 1000 });
const api = context.$.global.ISE_CLIP;
const requests = JSON.parse(fs.readFileSync(0, 'utf8'));
const results = requests.map(request => {
    try {
        // Explicit tagged replacements exercise IEEE nonfinite inputs, not JSON null.
        for (const path of request.replace || []) {
            let target = request.args;
            for (const index of path.slice(0, -1)) target = target[index];
            target[path[path.length - 1]] = Number(request.number);
        }
        const value = api[request.method || 'joinPolygon'].apply(api, request.args);
        function finite(v) {
            if (typeof v === 'number' && !Number.isFinite(v)) throw new Error('non-finite output');
            if (Array.isArray(v)) v.forEach(finite);
        }
        finite(value);
        return { value: value === undefined ? null : value };
    } catch (error) {
        return { error: error.message };
    }
});
process.stdout.write(JSON.stringify(results));
