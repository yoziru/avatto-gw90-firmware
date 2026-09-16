#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>

#include <esp_ota_ops.h>

#include "boot_guard.h"
#include "boot_guard_logic.h"
#include "config.h"
#include "web.h"
#include "zb.h"

namespace
{
constexpr const char *bootGuardNamespace = "bootguard";
constexpr const char *bootGuardArmedKey = "armed";
constexpr const char *bootGuardAttemptsKey = "tries";

Preferences bootGuardPrefs;
bool bootGuardArmed = false;
bool bootGuardFinished = false;
uint8_t bootGuardAttempts = 0;
uint32_t bootGuardBootMs = 0;
uint32_t bootGuardHealthySinceMs = 0;

void bootGuardDisarm()
{
    bootGuardPrefs.begin(bootGuardNamespace, false);
    bootGuardPrefs.putBool(bootGuardArmedKey, false);
    bootGuardPrefs.putUChar(bootGuardAttemptsKey, 0);
    bootGuardPrefs.end();
    bootGuardArmed = false;
}

void bootGuardRollback()
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *fallback = esp_ota_get_next_update_partition(running);
    esp_app_desc_t fallbackApp;

    if (fallback == nullptr)
    {
        printLogMsg("[BOOT] rollback skipped: no fallback app partition");
        bootGuardDisarm();
        bootGuardFinished = true;
        return;
    }
    if (esp_ota_get_partition_description(fallback, &fallbackApp) != ESP_OK)
    {
        printLogMsg("[BOOT] rollback skipped: fallback partition has no valid app");
        bootGuardDisarm();
        bootGuardFinished = true;
        return;
    }

    bootGuardDisarm();
    if (esp_ota_set_boot_partition(fallback) != ESP_OK)
    {
        printLogMsg("[BOOT] rollback failed: cannot set boot partition");
        bootGuardFinished = true;
        return;
    }

    printLogMsg(String("[BOOT] rolling back to ") + fallback->label + ", rebooting");
    delay(100);
    ESP.restart();
}

bool bootGuardHealthOk()
{
    const bool networkExpected = ConfigSettings.coordinator_mode != CoordinatorMode::PASSTHROUGH || ConfigSettings.keepWeb;
    if (networkExpected && !ConfigSettings.connectedEther && WiFi.status() != WL_CONNECTED)
    {
        return false;
    }
    if (ConfigSettings.coordinator_mode != CoordinatorMode::PASSTHROUGH && zbGetLastRxMs() == 0)
    {
        return false;
    }
    return true;
}
}

void bootGuardArm()
{
    bootGuardPrefs.begin(bootGuardNamespace, false);
    bootGuardPrefs.putBool(bootGuardArmedKey, true);
    bootGuardPrefs.putUChar(bootGuardAttemptsKey, 0);
    bootGuardPrefs.end();
}

void bootGuardBegin()
{
    bootGuardPrefs.begin(bootGuardNamespace, false);
    const bool armed = bootGuardPrefs.getBool(bootGuardArmedKey, false);
    if (!armed)
    {
        bootGuardPrefs.end();
        return;
    }
    bootGuardAttempts = static_cast<uint8_t>(bootGuardPrefs.getUChar(bootGuardAttemptsKey, 0) + 1);
    bootGuardPrefs.putUChar(bootGuardAttemptsKey, bootGuardAttempts);
    bootGuardPrefs.end();

    bootGuardArmed = true;
    bootGuardBootMs = millis();
    if (bootGuardAttempts > BOOT_GUARD_MAX_BOOT_ATTEMPTS)
    {
        printLogMsg(String("[BOOT] OTA boot guard: ") + String(bootGuardAttempts) + " attempts without health");
        bootGuardRollback();
        return;
    }
    printLogMsg(String("[BOOT] OTA boot guard: attempt ") + String(bootGuardAttempts) + "/" + String(BOOT_GUARD_MAX_BOOT_ATTEMPTS));
}

void bootGuardLoop()
{
    if (!bootGuardArmed || bootGuardFinished)
    {
        return;
    }

    const uint32_t now = millis();
    const bool healthy = bootGuardHealthOk();
    if (!healthy)
    {
        bootGuardHealthySinceMs = 0;
    }
    else if (bootGuardHealthySinceMs == 0)
    {
        bootGuardHealthySinceMs = now;
    }

    const uint32_t healthyMs = bootGuardHealthySinceMs == 0 ? 0 : now - bootGuardHealthySinceMs;
    const BootGuardDecision decision = bootGuardDecision(bootGuardArmed, bootGuardAttempts, healthy, healthyMs, now - bootGuardBootMs);

    if (decision == BootGuardDecision::Confirm)
    {
        bootGuardDisarm();
        bootGuardFinished = true;
        printLogMsg("[BOOT] OTA boot guard: health confirmed, guard cleared");
    }
    else if (decision == BootGuardDecision::Rollback)
    {
        printLogMsg(String("[BOOT] OTA boot guard: no health checkpoint after ") + String(bootGuardAttempts) + " attempt(s)");
        bootGuardRollback();
    }
}
