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
constexpr const char *bootGuardFailedKey = "rb_failed";

Preferences bootGuardPrefs;
bool bootGuardArmed = false;
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

    if (fallback == nullptr)
    {
        printLogMsg("[BOOT] rollback skipped: no fallback app partition");
        bootGuardDisarm();
        return;
    }
    if (esp_ota_set_boot_partition(fallback) != ESP_OK)
    {
        printLogMsg("[BOOT] rollback failed: cannot set boot partition");
        bootGuardDisarm();
        bootGuardPrefs.begin(bootGuardNamespace, false);
        bootGuardPrefs.putBool(bootGuardFailedKey, true);
        bootGuardPrefs.end();
        return;
    }

    bootGuardDisarm();
    printLogMsg(String("[BOOT] rolling back to ") + fallback->label + ", rebooting");
    delay(100);
    ESP.restart();
}

bool bootGuardHealthOk()
{
    if (networkExpected() && !ConfigSettings.connectedEther && WiFi.status() != WL_CONNECTED && !ConfigSettings.apStarted)
    {
        return false;
    }
    if (ConfigSettings.coordinator_mode != CoordinatorMode::PASSTHROUGH && zbGetLastClientTxMs() != 0 && zbGetLastRxMs() == 0)
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
    bootGuardPrefs.putBool(bootGuardFailedKey, false);
    bootGuardPrefs.end();
}

void bootGuardBegin()
{
    bootGuardPrefs.begin(bootGuardNamespace, false);
    const bool armed = bootGuardPrefs.getBool(bootGuardArmedKey, false);
    if (!armed)
    {
        if (bootGuardPrefs.getBool(bootGuardFailedKey, false))
        {
            printLogMsg("[BOOT] OTA boot guard: previous rollback could not switch slots");
        }
        bootGuardPrefs.end();
        return;
    }
    const uint8_t previousAttempts = bootGuardPrefs.getUChar(bootGuardAttemptsKey, 0);
    bootGuardAttempts = previousAttempts == UINT8_MAX ? previousAttempts : previousAttempts + 1;
    bootGuardPrefs.putUChar(bootGuardAttemptsKey, bootGuardAttempts);
    bootGuardPrefs.end();

    bootGuardArmed = true;
    bootGuardBootMs = millis();
    printLogMsg(String("[BOOT] OTA boot guard: attempt ") + String(bootGuardAttempts) + "/" + String(BOOT_GUARD_MAX_BOOT_ATTEMPTS));
    if (bootGuardDecision(true, bootGuardAttempts, false, 0, 0) == BootGuardDecision::Rollback)
    {
        printLogMsg("[BOOT] OTA boot guard: startup retry budget exhausted");
        bootGuardRollback();
    }
}

void bootGuardLoop()
{
    if (ConfigSettings.zbFlashing) return; // never reboot during coordinator erase/write
    if (!bootGuardArmed)
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
        printLogMsg("[BOOT] OTA boot guard: health confirmed, guard cleared");
    }
    else if (decision == BootGuardDecision::Rollback)
    {
        printLogMsg(String("[BOOT] OTA boot guard: no health checkpoint after ") + String(bootGuardAttempts) + " attempt(s)");
        bootGuardRollback();
    }
}
