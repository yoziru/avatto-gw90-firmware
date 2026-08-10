#include <Arduino.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>

#include "profiling.h"
#include "web.h"
#include "log.h"

// The on-device profiler is gated behind the debug build so production
// firmware carries no loop-timing or heap instrumentation. The pure stats math
// in profiling.h stays available for the native test suite.
#ifdef BUILD_DEBUG

namespace
{
LoopStats g_loopStats;
uint32_t g_lastReportMs = 0;
constexpr uint32_t kReportIntervalMs = 60000;
} // namespace

// Call at the top of loop(); the returned start timestamp is passed back to
// profilingLoopTick() at the bottom so the whole iteration is measured.
uint64_t profilingLoopBegin()
{
    return (uint64_t)esp_timer_get_time();
}

// Call at the bottom of loop(). Measures the full iteration (including the
// yield at the end), then every minute logs a snapshot and resets the window
// so each read reflects roughly the last 60 s of activity.
void profilingLoopTick(uint64_t cycleStartUs)
{
    const uint32_t cycleUs = (uint32_t)((uint64_t)esp_timer_get_time() - cycleStartUs);
    loopStatsSample(g_loopStats, cycleUs);

    const uint32_t nowMs = millis();
    if ((nowMs - g_lastReportMs) >= kReportIntervalMs)
    {
        g_lastReportMs = nowMs;
        if (g_loopStats.cycles > 0)
        {
            const size_t freeHeap = ESP.getFreeHeap();
            const size_t minFree = ESP.getMinFreeHeap();
            const size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
            const int fragPct = (freeHeap > 0) ? (int)(100 - (100ULL * largest / freeHeap)) : 0;
            printLogMsg(String("PROF loop min=") + g_loopStats.minUs + "us avg=" + g_loopStats.emaUs +
                        "us max=" + g_loopStats.maxUs + "us (" + g_loopStats.cycles +
                        " cyc) | heap free=" + freeHeap / 1024 + "K min=" + minFree / 1024 +
                        "K largest=" + largest / 1024 + "K frag=" + fragPct + "%");
        }
        loopStatsReset(g_loopStats);
    }
}

LoopStats profilingGetStats()
{
    return g_loopStats;
}

#endif // BUILD_DEBUG
