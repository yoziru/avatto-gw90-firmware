// Execute frontend contracts without DOM rendering or test-only exports.
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");
const calls = [];
const context = vm.createContext({
    document: { addEventListener() {} },
    localStorage: { getItem() { return null; } },
    window: {}, console, Headers,
    fetch: async (url, options) => {
        calls.push({ url, options });
        return { ok: true, json: async () => ({}), text: async () => "OK" };
    }
});
vm.runInContext(fs.readFileSync(path.join(__dirname, "../src/websrc/js/functions.js"), "utf8"), context);
(async () => {
    await vm.runInContext('gatewayFetch("/saveParams", {method: "POST", headers: {"Custom": "kept"}, body: "config"})', context);
    assert.equal(calls[0].options.headers.get("X-Gateway-Request"), "1");
    assert.equal(calls[0].options.headers.get("Custom"), "kept");
    assert.equal(calls[0].options.method, "POST");
    assert.equal(calls[0].options.body, "config");
    await vm.runInContext('fetchData("/api?action=1&param=zbRole", false)', context);
    assert.equal(calls[1].options.headers.get("X-Gateway-Request"), "1");
    await vm.runInContext('fetchData("https://api.github.com/repos/Koenkk/Z-Stack-firmware/releases")', context);
    assert.equal(calls[2].options, undefined); // never leak gateway headers to GitHub
    console.log("Frontend request contracts passed");
})().catch(error => { console.error(error); process.exitCode = 1; });
