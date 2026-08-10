#ifndef WIFI_SAVE_LOGIC_H_
#define WIFI_SAVE_LOGIC_H_

#include <stdint.h>
#include <string.h>

// Describes how the wifi save handler should treat an incoming /saveParams
// request for the Wi-Fi page.
//
// The core invariant: a NEW network is only saved after the STA connection to
// it actually succeeds. A wrong-password join must never become the saved
// network. An already-saved network may be updated immediately (its params are
// being tweaked, there is nothing new to validate), and "forget" always clears.
enum class WifiSaveMode : uint8_t {
    Forget,        // SSID empty/short -> clear saved config, drop STA connection
    SaveNow,       // SSID matches the saved network -> persist now, then reconnect
    ConnectFirst   // NEW network -> stage in RAM, persist only on successful connect
};

// Decide the mode from the request SSID and the currently saved SSID.
// `ssid` may be null/empty; `savedSsid` may be null when nothing is saved.
inline WifiSaveMode wifiSaveMode(const char *ssid, const char *savedSsid) noexcept
{
    if (ssid == nullptr || ssid[0] == '\0')
    {
        return WifiSaveMode::Forget;
    }
    // The handler treats an SSID shorter than 2 chars as "forget".
    if (ssid[1] == '\0')
    {
        return WifiSaveMode::Forget;
    }
    if (savedSsid != nullptr && strcmp(ssid, savedSsid) == 0)
    {
        return WifiSaveMode::SaveNow;
    }
    return WifiSaveMode::ConnectFirst;
}

// True when the request persists immediately instead of waiting for a connect.
inline bool wifiSavePersistsNow(WifiSaveMode mode) noexcept
{
    return mode != WifiSaveMode::ConnectFirst;
}

// True when a staged (pending) save may be committed: the STA link is up and it
// is connected to the very network we staged. Any other association (or no
// link at all) must NOT commit the pending config.
inline bool wifiShouldCommit(bool pendingActive, bool connected, const char *connectedSsid, const char *pendingSsid) noexcept
{
    if (!pendingActive || !connected || connectedSsid == nullptr || pendingSsid == nullptr)
    {
        return false;
    }
    return strcmp(connectedSsid, pendingSsid) == 0;
}

// Decide whether to (re)issue WiFi.begin(). While the radio is mid-connect
// (WL_IDLE_STATUS) and the last attempt is recent, the caller must NOT restart
// the connection - that would hammer the AP on every overseer tick. A stuck
// in-progress attempt older than the throttle window is retried.
inline bool wifiShouldBeginConnect(bool connecting, uint32_t lastBeginAgeMs) noexcept
{
    return !(connecting && lastBeginAgeMs < 20000);
}

#endif
