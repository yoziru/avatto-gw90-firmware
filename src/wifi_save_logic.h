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
    Forget,        // SSID empty -> clear saved config, drop STA connection
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
    if (savedSsid != nullptr && strcmp(ssid, savedSsid) == 0)
    {
        return WifiSaveMode::SaveNow;
    }
    return WifiSaveMode::ConnectFirst;
}

inline bool wifiHasCredentials(const char *ssid, const char *password) noexcept
{
    if (!ssid || !password || strlen(ssid) == 0 || strlen(ssid) > 32) return false;
    const size_t length = strlen(password);
    return length == 0 || (length >= 8 && length <= 63);
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

// True when a staged join must be abandoned instead of retried forever: the
// STA connected to a different network for a while (the saved one won the
// race), or never connected at all within the timeout (wrong password, AP out
// of range). A short grace period guards the first seconds, where WiFi.SSID()
// may still report the previous association while the new one is starting.
inline bool wifiPendingGiveUp(bool connectedToPending, bool connectedToOther, uint32_t ageMs) noexcept
{
    if (connectedToPending)
    {
        return false;
    }
    if (connectedToOther)
    {
        return ageMs > 15000;
    }
    return ageMs > 45000;
}

#endif
