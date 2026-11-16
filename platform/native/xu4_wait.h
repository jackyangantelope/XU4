#pragma once
#include <stdint.h>

// Unsigned subtraction also handles the millisecond clock wrapping.
static inline unsigned xu4_wait_remaining(uint32_t start, uint32_t now, unsigned duration) {
    const uint32_t elapsed = now - start;
    return elapsed < duration ? duration - elapsed : 0;
}

// Keep a recent action presentation, but never suppress an already due frame.
static inline bool xu4_outer_present_needed(unsigned before, unsigned after,
                                            bool have_swap, uint32_t last, uint32_t now,
                                            unsigned period) {
    return before == after || !have_swap || xu4_wait_remaining(last, now, period) == 0;
}

#ifdef XU4_PICO
void xu4_complete_begin();
void xu4_complete_end();
struct Xu4CompleteScope {
    Xu4CompleteScope(){xu4_complete_begin();}
    ~Xu4CompleteScope(){xu4_complete_end();}
    Xu4CompleteScope(const Xu4CompleteScope&)=delete;
    Xu4CompleteScope& operator=(const Xu4CompleteScope&)=delete;
};
extern "C" void xu4_wait_until_us(uint64_t deadline);
unsigned xu4_action_frame_count();
bool xu4_outer_present_needed(unsigned action_frames_before);
#endif
