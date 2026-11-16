#pragma once
#include <stdint.h>
// Fixed phase in microseconds. Skip expired slots, never burst to catch up.
struct Xu4PresentClock {
    uint64_t deadline=0, period=40000, missed=0, late_max=0;
    bool started=false;
    unsigned remaining(uint64_t now) const {
        return !started ? UINT32_MAX : now>=deadline ? 0 : unsigned((deadline-now+999)/1000);
    }
    void begin(uint64_t now) {
        if (!started) {started=true;deadline=now+period;return;}
        uint64_t late=now>deadline?now-deadline:0;
        if(late>late_max)late_max=late;
        missed+=late/period;
        deadline+=(late/period+1)*period;
    }
};
