// Execute frontend contracts without DOM rendering or test-only exports.
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");
const calls = [];
let responseOk = true;
const context = vm.createContext({
    document: { addEventListener() {} },
    localStorage: { getItem() { return null; } },
    window: {}, console, Headers,
    fetch: async (url, options) => {
        calls.push({ url, options });
        return { ok: responseOk, status: responseOk ? 200 : 500, json: async () => ({}), text: async () => "OK" };
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
    responseOk = false;
    await assert.rejects(vm.runInContext('gatewayFetch("/saveParams", {method: "POST"})', context), /HTTP 500/);
    responseOk = true;
    for (const [remote, local, newer] of [
        ["v1.10.0", "1.9.9", true], ["v2.0.0", "1.99.99", true],
        ["v1.2.3", "1.2.3-dirty+g123456", false], ["1.2.4", "1.2.3+g999999", true],
        ["1.2", "1.2.0", false], ["20260101", "20251231", true],
        ["unknown", "1.0.0", false], ["1.0.0", "unknown", true]
    ]) assert.equal(vm.runInContext(`verIsNewer(${JSON.stringify(remote)}, ${JSON.stringify(local)})`, context), newer);
    for (const [mode, label] of [[0, "OPEN"], [1, "WEP"], [5, "WPA2 Enterprise"], [6, "WPA3"], [7, "WPA2/WPA3"]])
        assert.equal(vm.runInContext(`wifiSecurityLabel(${mode})`, context), label);
    assert.equal(vm.runInContext('GITHUB_REPO_OWNER', context), "yoziru");

    // Exercise the real install handler: pending feedback, HTTP failures and a
    // request that never resolves must not leave the page silently stuck.
    const installElements = {}, timers = new Map();
    let timerId = 0, pendingResolve, pendingReject, closed = 0, installReboots = 0;
    const eventHandlers = {};
    Object.assign(context, {
        AbortController,
        getEl: id => installElements[id] ||= { style: {}, classList: Object.assign(new Set(), {
            remove(name) { this.delete(name); }
        }) },
        t: key => key,
        setTimeout: (callback, delay) => { timers.set(++timerId, { callback, delay }); return timerId; },
        clearTimeout: id => timers.delete(id),
        EventSource: class { addEventListener(name, handler) { eventHandlers[name] = handler; } close() { ++closed; } },
        rebootWait: () => ++installReboots,
        fetch: (url, options) => new Promise((resolve, reject) => {
            pendingResolve = resolve;
            pendingReject = reject;
            options.signal.addEventListener("abort", () => reject(Object.assign(new Error("aborted"), { name: "AbortError" })));
        })
    });
    const flush = () => new Promise(resolve => setImmediate(resolve));
    for (const timeoutCase of [false, true]) {
        vm.runInContext("espFlashGitWait()", context);
        const start = [...timers.values()].find(timer => timer.delay === 500);
        timers.clear(); start.callback();
        assert.equal(installElements.updEspGit.disabled, true);
        assert.equal(installElements.bar.classList.has("busy-shimmer"), true);
        assert.equal(installElements.bar.style.width, "100%");
        if (timeoutCase) [...timers.values()][0].callback();
        else pendingResolve({ ok: false, status: 403 });
        await flush();
        assert.match(installElements.prg.innerHTML, timeoutCase ? /may still be updating/ : /HTTP 403/);
        assert.equal(installElements.updEspGit.disabled, false);
        assert.equal(installElements.bar.classList.has("busy-shimmer"), false);
        assert.equal(timers.size, 0);
    }
    assert.equal(closed, 2);
    vm.runInContext("espFlashGitWait()", context);
    const start = [...timers.values()].find(timer => timer.delay === 500);
    timers.clear(); start.callback();
    eventHandlers.ESP_FW_prgs({ data: "100" });
    assert.equal(installReboots, 0); // downloaded is not signature-verified or finalized
    eventHandlers.ESP_FW_info({ data: "Verifying firmware..." });
    assert.equal(installElements.prg.textContent, "Verifying firmware...");
    eventHandlers.ESP_FW_done({ data: "ok" });
    assert.equal(installReboots, 1);
    pendingReject(new TypeError("connection closed by reboot"));
    await flush();
    assert.equal(installElements.prg.innerHTML, "d.updDoneReboot");
    assert.equal(timers.size, 0);

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
