#pragma once
#include <stdint.h>
// Each block is 10ms of PCM. Preserve at least 40ms before yielding for video.
// A single decoder call is indivisible; these are soft service budgets.
static inline unsigned xu4_audio_yield_reason(unsigned queued, unsigned blocks,
                                               uint64_t elapsed_us, unsigned frame_left_ms) {
    if (queued < 1764) return 0;
    if (frame_left_ms <= 6) return 1;
    if (blocks >= 2 || elapsed_us >= 6000) return 2;
    return 0;
}
// Cover the measured ~9.5ms framebuffer copy plus the next audio block.
// Still a hardware-tested margin, not a guarantee for an unbounded decoder call.
static inline bool xu4_audio_video_ready(unsigned queued, unsigned remaining_ms) {
    return remaining_ms == 0 && queued >= 882;
}
// Only non-emergency audio work may yield when the enclosing wait expires.
static inline bool xu4_wait_audio_can_yield(unsigned queued,int64_t wait_left_us){
    return wait_left_us<=0 && queued>=1764;
}
#ifdef XU4_PICO
unsigned xu4_video_remaining_ms();

#endif
