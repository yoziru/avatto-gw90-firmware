#ifndef PROFILING_H_
#define PROFILING_H_

#include <stdint.h>

// Lightweight loop() timing. The ESP32 main loop is a strict serial scheduler:
// the web server, Zigbee socket bridge, MQTT and the temp state machine all run
// in one iteration, so the loop cycle time is the end-to-end latency of the
// gateway. We track min/max (worst spikes) plus an EMA that reflects the
// steady-state cost of the loop.

struct LoopStats
{
    uint32_t minUs = UINT32_MAX;
    uint32_t maxUs = 0;
    uint32_t emaUs = 0;  // reported "typical" loop cost (microseconds)
    uint32_t emaAcc = 0; // internal fixed-point accumulator (x128)
    uint32_t cycles = 0;
};

inline void loopStatsReset(LoopStats &s) noexcept
{
    s.minUs = UINT32_MAX;
    s.maxUs = 0;
    s.emaUs = 0;
    s.emaAcc = 0;
    s.cycles = 0;
}

// Feed one measured loop duration (microseconds). The EMA uses a x128 fixed
// point accumulator so it converges exactly (a plain integer shift would floor
// ~127 us below the steady value) while a single spike only moves it by
// delta/128 per sample.
inline void loopStatsSample(LoopStats &s, uint32_t us) noexcept
{
    if (us < s.minUs)
    {
        s.minUs = us;
    }
    if (us > s.maxUs)
    {
        s.maxUs = us;
    }
    s.emaAcc = s.emaAcc + (uint32_t)(((int64_t)us * 128 - (int64_t)s.emaAcc) >> 7);
    s.emaUs = (s.emaAcc + 64) >> 7;
    s.cycles++;
}

// Device-bound helpers implemented in profiling.cpp (esp_timer + heap_caps).
// profilingLoopBegin()/profilingLoopTick() bracket each loop() iteration;
// profilingGetStats() returns the current minute's window for the status page.
uint64_t profilingLoopBegin();
void profilingLoopTick(uint64_t cycleStartUs);
LoopStats profilingGetStats();

#endif
