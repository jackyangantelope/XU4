#pragma once
#include <stdint.h>
enum Xu4ProfileCategory {
    XPROF_INPUT, XPROF_TICK, XPROF_AUDIO, XPROF_TRACK, XPROF_DECODE,
    XPROF_COPY, XPROF_SLEEP, XPROF_OPEN, XPROF_READ, XPROF_SEEK, XPROF_UART,
    XPROF_GAME_FIRST, XPROF_KEYTURN = XPROF_GAME_FIRST, XPROF_TURN, XPROF_PARTY,
    XPROF_EFFECT, XPROF_OBJECTS, XPROF_SPAWN, XPROF_DRAW, XPROF_MAPDRAW,
    XPROF_TIMER, XPROF_MOVE,
    XPROF_TILE_FIRST, XPROF_TILE_LAYER = XPROF_TILE_FIRST, XPROF_TILE_ANIM,
    XPROF_TILE_STATIC, XPROF_TILE_SUBMIT, XPROF_TILE_COPY, XPROF_TILE_BLEND, XPROF_COUNT
};
#ifdef XU4_PICO
#ifdef __cplusplus
extern "C" {
#endif
void xu4_prof_enter(enum Xu4ProfileCategory category);
void xu4_prof_leave(enum Xu4ProfileCategory category);
void xu4_prof_frame(uint64_t now);
void xu4_prof_report(uint64_t now);
void xu4_prof_uart_init(void);
void xu4_uart_service(void);
void xu4_uart_enable_queue(void);
void xu4_prof_key(void);
void xu4_prof_audio_recovery(unsigned queue_start, unsigned blocks, uint64_t block_max_us, unsigned presents, uint64_t video_us, unsigned defers);
void xu4_prof_audio_yield(unsigned reason);
void xu4_prof_audio_call(uint64_t elapsed, unsigned blocks, const uint64_t parts[4]);
void xu4_prof_skip_age(uint32_t age_ms, unsigned budget_ms);
void xu4_note_skipped_present(void);
#ifdef __cplusplus
}
class Xu4ProfileScope {
    Xu4ProfileCategory category;
public:
    explicit Xu4ProfileScope(Xu4ProfileCategory value) : category(value) { xu4_prof_enter(category); }
    ~Xu4ProfileScope() { xu4_prof_leave(category); }
    Xu4ProfileScope(const Xu4ProfileScope&) = delete;
    Xu4ProfileScope& operator=(const Xu4ProfileScope&) = delete;
};
class Xu4WaitScope {
    bool action;
    unsigned requested_ms;
    uint64_t started, audio_started;
public:
    Xu4WaitScope(bool is_action, unsigned requested);
    ~Xu4WaitScope();
    Xu4WaitScope(const Xu4WaitScope&) = delete;
    Xu4WaitScope& operator=(const Xu4WaitScope&) = delete;
};
void xu4_prof_action_frame();
#define XU4_PROFILE_SCOPE(category) Xu4ProfileScope profile_scope(category)
#endif
#else
#define XU4_PROFILE_SCOPE(category) ((void)0)
#define xu4_prof_key() ((void)0)
#endif
