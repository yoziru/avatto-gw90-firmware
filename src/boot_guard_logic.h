#ifndef BOOT_GUARD_LOGIC_H_
#define BOOT_GUARD_LOGIC_H_

#include <stdint.h>

// Anti-brick guard for OTA updates. The device only arms the guard right
// before an OTA reboot. While armed, each boot is an attempt that must reach
// the health checkpoint (network + coordinator) and hold it for a settling
// period. A boot that crashes before confirming spends its attempt; a boot
// that stays up without reaching the checkpoint is given the same timeout.
// Once the attempts are used up, the guard rolls back to the other slot.
constexpr uint8_t BOOT_GUARD_MAX_BOOT_ATTEMPTS = 3;
constexpr uint32_t BOOT_GUARD_SETTLE_MS = 60000;
constexpr uint32_t BOOT_GUARD_ATTEMPT_TIMEOUT_MS = 5 * 60 * 1000;

enum class BootGuardDecision : uint8_t
{
    Wait,
    Confirm,
    Rollback
};

inline BootGuardDecision bootGuardDecision(bool armed, uint8_t attempts, bool healthy,
                                           uint32_t healthyMs, uint32_t attemptAgeMs) noexcept
{
    if (!armed)
    {
        return BootGuardDecision::Wait;
    }
    if (healthy && healthyMs >= BOOT_GUARD_SETTLE_MS)
    {
        return BootGuardDecision::Confirm;
    }
    if (attempts > BOOT_GUARD_MAX_BOOT_ATTEMPTS || attemptAgeMs >= BOOT_GUARD_ATTEMPT_TIMEOUT_MS)
    {
        return BootGuardDecision::Rollback;
    }
    return BootGuardDecision::Wait;
}

#endif
