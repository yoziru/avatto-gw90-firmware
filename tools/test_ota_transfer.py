#!/usr/bin/env python3
"""Execute the OTA transfer owner with deterministic HTTP/Update boundaries."""
import os
import re
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
# Hardware regression: measured TLS peak exceeds Arduino's default 8192 bytes.
stack = re.search(r'SET_LOOP_TASK_STACK_SIZE\((\d+)\)', (ROOT / 'src/main.cpp').read_text())
assert stack and int(stack[1]) >= 16384, 'HTTPS OTA needs at least 16 KiB of loop-task stack'
source = (ROOT / "src/web/ota.cpp").read_text()
routine = source[source.index("static bool runEspUpdateFirmware("):]
harness = r"""
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>
#include "fw_verify.h"
using std::min;
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#define HTTPC_STRICT_FOLLOW_REDIRECTS 1
#define HTTP_CODE_OK 200
#define UPDATE_SIZE_UNKNOWN size_t(-1)
struct String : std::string {
    using std::string::string;
    String(std::string s) : std::string(s) {}
    String(int value) : std::string(std::to_string(value)) {}
    bool startsWith(const char *prefix) const { return rfind(prefix, 0) == 0; }
    size_t length() const { return size(); }
};
std::vector<uint8_t> body, written;
int contentLength = 4, response = 200;
unsigned clockMs = 0, bridgeTicks = 0;
size_t cursor = 0;
bool stall = false, beginOk = true, writeOk = true, finalizeOk = true, signatureOk = true;
bool secureSelected = false, armed = false, aborted = false, signatureChecked = false, finalized = false;
unsigned long millis() { return clockMs; }
void delay(unsigned ms) { clockMs += ms; }
void processSocketBridge() { ++bridgeTicks; }
void printLogMsg(const String &) {}
std::vector<std::string> events;
void sendEvent(const char *name, unsigned, const String &) {
    if (std::string(name) == "ESP_FW_done") assert(armed && finalized);
    events.push_back(name);
}
void progressFunc(unsigned, unsigned) {}
struct NetworkClient {
    bool secure = false;
    size_t available() { return body.size() - cursor; }
    int readBytes(uint8_t *out, size_t n) {
        n = min(n, available()); std::copy_n(body.data() + cursor, n, out); cursor += n; return n;
    }
    void setTimeout(unsigned) {}
};
struct NetworkClientSecure : NetworkClient { NetworkClientSecure() { secure = true; } void setHandshakeTimeout(unsigned) {} };
void trustEspCertificateBundle(NetworkClientSecure &) {}
std::string encoding;
struct HTTPClient {
    NetworkClient *transport = nullptr;
    bool begin(NetworkClient &client, const String &) { transport = &client; secureSelected = client.secure; return true; }
    void setFollowRedirects(int) {}
    void setTimeout(unsigned) {}
    void setConnectTimeout(unsigned) {}
    static String errorToString(int) { return "connection refused"; }
    void useHTTP10(bool) {}
    void collectHeaders(const char **, size_t) {}
    void addHeader(const char *, const char *) {}
    int GET() { return response; }
    int getSize() { return contentLength; }
    String header(const char *) { return encoding; }
    bool connected() { return stall || cursor < body.size(); }
    NetworkClient *getStreamPtr() { return transport; }
    void end() {}
};
using WiFiClient = NetworkClient;
struct {
    bool begin(size_t) { return beginOk; }
    void onProgress(void (*)(unsigned, unsigned)) {}
    size_t write(uint8_t *data, size_t n) {
        if (!writeOk) return 0; written.insert(written.end(), data, data + n); return n;
    }
    bool end(bool) { finalized = finalizeOk; return finalizeOk; }
    void abort() { aborted = true; }
} Update;
int fwSha256;
bool fwHashActive = false;
const char *fwRejectError = "bad signature", *fwIncompleteError = "firmware download incomplete";
char fwSigUrlBuf[256];
int totalLength, currentLength;
void fwHashAbort() { fwHashActive = false; }
void mbedtls_sha256_init(int *) {}
int mbedtls_sha256_starts(int *, int) { return 0; }
int mbedtls_sha256_update(int *, uint8_t *, size_t) { return 0; }
bool verifyFirmwareSignature() { signatureChecked = true; fwHashAbort(); fwRejectError = "bad signature"; return signatureOk; }
void bootGuardArm() { armed = true; }
struct Reboot {};
void esp_restart() { throw Reboot{}; }
"""
checks = r"""
int main() {
    auto reset = [] {
        body = {0xE9, 1, 2, 3}; written.clear(); contentLength = 4; response = 200;
        clockMs = bridgeTicks = cursor = 0; stall = false;
        beginOk = writeOk = finalizeOk = signatureOk = true;
        armed = aborted = signatureChecked = finalized = false; encoding.clear(); events.clear();
    };
    auto succeeds = [](const char *url, bool force) {
        bool rebooted = false;
        try { getEspUpdate(url, force); } catch (Reboot &) { rebooted = true; }
        assert(rebooted && armed && written == body);
        assert(events.back() == "ESP_FW_done");
    };
    reset(); succeeds("http://server/firmware.bin", true); assert(!secureSelected);
    reset(); contentLength = -1; succeeds("https://server/firmware.bin", false);
    assert(secureSelected && signatureChecked);
    reset(); contentLength = 5;
    assert(getEspUpdate("http://server/fw.bin", true) == "firmware download incomplete" && aborted && !armed);
    reset(); body.clear(); stall = true;
    getEspUpdate("http://server/fw.bin", true);
    assert(aborted && !armed && clockMs <= 15002 && bridgeTicks > 0);
    reset(); writeOk = false; getEspUpdate("http://server/fw.bin", true); assert(aborted && !armed);
    reset(); beginOk = false; getEspUpdate("http://server/fw.bin", true); assert(written.empty() && !armed);
    reset(); signatureOk = false; getEspUpdate("https://server/fw.bin", false); assert(aborted && !armed && signatureChecked);
    assert(std::find(events.begin(), events.end(), "ESP_FW_done") == events.end());
    reset(); finalizeOk = false;
    assert(getEspUpdate("https://server/fw.bin", false) == "update finalize failed" && !armed);
    assert(std::find(events.begin(), events.end(), "ESP_FW_done") == events.end());
    reset(); response = -1;
    assert(getEspUpdate("https://server/fw.bin", false) == "firmware download failed: connection refused" && written.empty() && !armed);
    reset(); response = 404;
    assert(getEspUpdate("https://server/fw.bin", false) == "firmware download failed: HTTP 404" && written.empty() && !armed);
    reset(); encoding = "chunked"; getEspUpdate("http://server/fw.bin", true); assert(written.empty() && !armed);
    reset(); getEspUpdate("file:///fw.bin", true); assert(written.empty() && !armed);
}
"""
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    (work / "test.cpp").write_text(harness + routine + checks)
    subprocess.run([os.environ.get("CXX", "c++"), "-std=c++17", "-fsanitize=address,undefined",
                    "-I" + str(ROOT / "src"), str(work / "test.cpp"), "-o", str(work / "check")], check=True)
    subprocess.run([str(work / "check")], check=True)
print("OTA transport/deadline/error regressions passed (ASan/UBSan)")
