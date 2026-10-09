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

    const handlers = {}, elements = {}, uploads = [];
    let reboots = 0;
    const page = vm.createContext({
        console, localStorage: { getItem() { return null; } }, window: { confirm() { return true; } },
        t: key => key, FormData: class {}, MutationObserver: class { observe() {} },
        getEl: id => elements[id] ||= { style: {}, removeAttribute() { this.disabled = false; } },
        getQ: selector => ({ addEventListener(event, handler) { handlers[selector] = handler; } }),
        rebootWait: () => ++reboots,
        XMLHttpRequest: class {
            constructor() { this.upload = { addEventListener() {} }; this.headers = {}; uploads.push(this); }
            open(method, url) { this.method = method; this.url = url; }
            setRequestHeader(key, value) { this.headers[key] = value; }
            send() {}
        }
    });
    const html = fs.readFileSync(path.join(__dirname, "../src/websrc/html/PAGE_ABOUT.html"), "utf8");
    vm.runInContext(html.match(/<script>([\s\S]*?)<\/script>/)[1], page);
    for (const [status, responseText, accepted] of [[200, "FAIL", false], [500, "write failed", false], [200, "OK", true]]) {
        const before = reboots;
        handlers["form#upload_form"].call({}, { preventDefault() {} });
        const xhr = uploads.at(-1);
        assert.equal(xhr.headers["X-Gateway-Request"], "1");
        xhr.status = status; xhr.responseText = responseText; xhr.onload();
        assert.equal(reboots - before, accepted ? 1 : 0);
        assert.equal(elements.updButton.disabled, accepted);
    }
    console.log("Frontend request contracts passed");
})().catch(error => { console.error(error); process.exitCode = 1; });
