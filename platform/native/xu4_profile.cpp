#include "xu4_timeline.h"
#include "xu4_profile.h"
#include "xu4_draw_trace.h"
#include <initializer_list>
#include "pico/time.h"
#include "pico/stdio_uart.h"
#include "pico/stdio/driver.h"
#include "pico/critical_section.h"
#include "hardware/uart.h"
#include "xu4_uart_queue.h"
#include <cstdio>
#include <cstring>

// Inclusive categories may overlap. Active scopes are split at frame boundaries,
// including nested event loops that present before their caller returns.
struct CategoryClock {
    unsigned depth;
    uint64_t start, elapsed;
};
static CategoryClock clocks[XPROF_COUNT];
static uint64_t totals[XPROF_COUNT], worst[XPROF_COUNT];
static uint64_t game_started[XPROF_COUNT], game_max[XPROF_COUNT];
static unsigned game_calls[XPROF_COUNT];
static uint64_t row_fast_calls,row_fast_rows,row_fast_pixels,row_fallback_calls,row_fallback_pixels;
extern "C" void xu4_prof_rowcopy(unsigned fast,unsigned rows,unsigned pixels){
 if(fast){++row_fast_calls;row_fast_rows+=rows;row_fast_pixels+=pixels;}
 else{++row_fallback_calls;row_fallback_pixels+=pixels;}
}
static uint64_t blit_opaque,blit_clear,blit_partial,blit_copy;
extern "C" void xu4_prof_blit(unsigned opaque,unsigned clear,unsigned partial,unsigned copy) {
 blit_opaque+=opaque;blit_clear+=clear;blit_partial+=partial;blit_copy+=copy;
}
static uint64_t previous_frame, worst_gap, worst_at;
static unsigned over100, over500;
static unsigned frame_keys, window_keys, max_keys, worst_keys;
static unsigned frame_sleeps, worst_sleeps;
static uint64_t audio_completed;
struct WaitStats {
    unsigned calls;
    uint64_t requested, actual, audio, maximum;
    uint64_t over, over_max;
};
struct AudioCallStats {
    unsigned calls, empty, blocks, blocks_max, slow_blocks;
    uint64_t total, maximum, parts[4], slow_parts[4];
};
static AudioCallStats audio_calls;
static unsigned recovery_calls, recovery_low_calls, recovery_queue_min = UINT32_MAX, recovery_presents, recovery_defers;
static uint64_t recovery_block_max, recovery_video_us;
extern "C" void xu4_prof_audio_recovery(unsigned queue_start, unsigned blocks, uint64_t block_max_us, unsigned presents, uint64_t video_us, unsigned defers) {
    if (blocks) {
        ++recovery_calls;
        if (queue_start < 1764) ++recovery_low_calls;
        if (queue_start < recovery_queue_min) recovery_queue_min = queue_start;
        if (block_max_us > recovery_block_max) recovery_block_max = block_max_us;
    }
    recovery_defers += defers;
    recovery_presents += presents;
    recovery_video_us += video_us;
}
static unsigned audio_deadline_yields, audio_budget_yields, audio_low_blocks;
extern "C" void xu4_prof_audio_yield(unsigned reason) {
    if (reason == 1) ++audio_deadline_yields;
    else if (reason == 2) ++audio_budget_yields;
    else if (reason == 3) ++audio_low_blocks;
}
static unsigned present_skips, late_skips, skip_age_max;
extern "C" void xu4_prof_audio_call(uint64_t elapsed, unsigned blocks, const uint64_t parts[4]) {
    ++audio_calls.calls;
    if (!blocks) ++audio_calls.empty;
    audio_calls.blocks += blocks;
    if (blocks > audio_calls.blocks_max) audio_calls.blocks_max = blocks;
    audio_calls.total += elapsed;
    for (unsigned i=0;i<4;++i) audio_calls.parts[i] += parts[i];
    if (elapsed > audio_calls.maximum) {
        audio_calls.maximum = elapsed;
        audio_calls.slow_blocks = blocks;
        memcpy(audio_calls.slow_parts, parts, sizeof audio_calls.slow_parts);
    }
}
extern "C" void xu4_prof_skip_age(uint32_t age_ms, unsigned budget_ms) {
    ++present_skips;
    if (age_ms >= budget_ms) ++late_skips;
    if (age_ms > skip_age_max) skip_age_max = age_ms;
}
static WaitStats action_wait, loop_wait;
static unsigned action_frames, loop_skips;
static uint64_t audio_clock() {
    const auto &clock = clocks[XPROF_AUDIO];
    return audio_completed + (clock.depth ? time_us_64() - clock.start : 0);
}
Xu4WaitScope::Xu4WaitScope(bool is_action, unsigned requested)
    : action(is_action), requested_ms(requested), started(time_us_64()), audio_started(audio_clock()) {}
Xu4WaitScope::~Xu4WaitScope() {
    auto &stats = action ? action_wait : loop_wait;
    const uint64_t elapsed = time_us_64() - started;
    ++stats.calls;
    stats.requested += uint64_t(requested_ms) * 1000;
    stats.actual += elapsed;
    stats.audio += audio_clock() - audio_started;
    if (elapsed > stats.maximum) stats.maximum = elapsed;
    const uint64_t requested_us = uint64_t(requested_ms) * 1000;
    if (elapsed > requested_us) {
        const uint64_t over = elapsed - requested_us;
        stats.over += over;
        if (over > stats.over_max) stats.over_max = over;
    }
    if (!action && !requested_ms) ++loop_skips;
}
void xu4_prof_action_frame() { ++action_frames; }
static void (*uart_out)(const char *, int);
static void (*uart_flush)(void);
static Xu4UartQueue uart_queue;
static critical_section_t uart_lock;
static bool uart_queued;
extern "C" void xu4_uart_service() {
    if (!uart_queued) return;
    XU4_PROFILE_SCOPE(XPROF_UART);
    critical_section_enter_blocking(&uart_lock);
    // Sole TX writer after enabling the queue. With a writable FIFO, a one-byte
    // raw write cannot wait for wire transmission; do not call tx_wait_blocking.
    uart_queue.drain([] { return uart_is_writable(uart_default); },
                     [](char c) { uart_putc_raw(uart_default, c); });
    critical_section_exit(&uart_lock);
}
extern "C" void xu4_uart_enable_queue() {
    if (uart_queued) return;
    if (uart_flush) uart_flush(); // startup only, before queued measurement
    uart_queued = true;
}
static void measured_uart_out(const char *data, int length) {
    XU4_PROFILE_SCOPE(XPROF_UART);
    if (uart_queued) {
        if (length > 0) {
            critical_section_enter_blocking(&uart_lock);
            uart_queue.push(data, size_t(length));
            critical_section_exit(&uart_lock);
        }
        return;
    }
    uart_out(data, length);
}
static void measured_uart_flush() {
    if (uart_queued) { xu4_uart_service(); return; }
    XU4_PROFILE_SCOPE(XPROF_UART);
    if (uart_flush) uart_flush();
}
extern "C" void xu4_prof_uart_init() {
    if (uart_out) return;
    critical_section_init(&uart_lock);
    uart_out = stdio_uart.out_chars;
    uart_flush = stdio_uart.out_flush;
    stdio_uart.out_chars = measured_uart_out;
    stdio_uart.out_flush = measured_uart_flush;
}
extern "C" void xu4_prof_key() { ++frame_keys; }
static const char *names[] = {
    "input", "tick", "audio", "track", "decode", "copy", "sleep",
    "open", "read", "seek", "uart",
    "keyturn", "turn", "party", "effect", "objects", "spawn", "draw", "mapdraw", "timer", "move",
    "layer", "anim", "static", "submit", "copy32", "blend32"
};
extern "C" void xu4_prof_enter(Xu4ProfileCategory category) {
    if (category == XPROF_SLEEP) ++frame_sleeps;
    auto &clock = clocks[category];
    if (!clock.depth++) {
        clock.start = time_us_64();
        if(category<XPROF_TILE_FIRST)xu4_timeline_begin(category,clock.start);
        if (category >= XPROF_GAME_FIRST) game_started[category] = clock.start;
    }
}
extern "C" void xu4_prof_leave(Xu4ProfileCategory category) {
    auto &clock = clocks[category];
    if (!--clock.depth) {
        const uint64_t finished = time_us_64();
        if(category<XPROF_TILE_FIRST)xu4_timeline_end(category,finished);
        const uint64_t elapsed = finished - clock.start;
        if (category >= XPROF_GAME_FIRST) {
            ++game_calls[category];
            const uint64_t whole = finished - game_started[category];
            if (whole > game_max[category]) game_max[category] = whole;
        }
        clock.elapsed += elapsed;
        if (category == XPROF_AUDIO) audio_completed += elapsed;
    }
}
extern "C" void xu4_prof_frame(uint64_t now) {
    xu4_timeline_frame(now);
    const uint64_t gap = previous_frame ? now - previous_frame : 0;
    const bool slowest = gap > worst_gap;
    for (unsigned i = 0; i < XPROF_COUNT; ++i) {
        auto &clock = clocks[i];
        if (clock.depth) {
            const uint64_t elapsed = now - clock.start;
            clock.elapsed += elapsed;
            if (i == XPROF_AUDIO) audio_completed += elapsed;
            clock.start = now;
        }
        if (previous_frame) {
            totals[i] += clock.elapsed;
            if (slowest) worst[i] = clock.elapsed;
        }
        clock.elapsed = 0;
    }
    xu4_draw_frame(slowest,previous_frame!=0);
    if (slowest) {
        worst_gap = gap; worst_at = now;
        worst_keys = frame_keys; worst_sleeps = frame_sleeps;
    }
    if (previous_frame) {
        window_keys += frame_keys;
        if (frame_keys > max_keys) max_keys = frame_keys;
    }
    frame_keys = frame_sleeps = 0;
    if (gap > 100000) ++over100;
    if (gap > 500000) ++over500;
    previous_frame = now;
}
extern "C" void xu4_prof_report(uint64_t now) {
    critical_section_enter_blocking(&uart_lock);
    const size_t queued = uart_queue.used, peak = uart_queue.peak;
    const uint64_t dropped = uart_queue.dropped, sent = uart_queue.sent;
    critical_section_exit(&uart_lock);
    printf("XU4_UART ms=%lu mode=queued pending=%u peak=%u capacity=8192 sent_total=%llu dropped_total=%llu service_budget=16\n",
           (unsigned long)(now / 1000), unsigned(queued), unsigned(peak),
           (unsigned long long)sent, (unsigned long long)dropped);
    // Assemble one line so Pico's printf wrapper flushes only once.
    char line[768];
    size_t used = snprintf(line, sizeof line,
           "XU4_SLOW ms=%lu at_ms=%lu gap_us=%llu over100=%u over500=%u keys=%u keys_max=%u slow_keys=%u slow_sleeps=%u uart_win_us=%llu",
           (unsigned long)(now / 1000), (unsigned long)(worst_at / 1000),
           (unsigned long long)worst_gap, over100, over500,
           window_keys, max_keys, worst_keys, worst_sleeps,
           (unsigned long long)totals[XPROF_UART]);
    for (unsigned i = 0; i < XPROF_GAME_FIRST; ++i)
        used += snprintf(line + used, sizeof line - used,
                         " %s_us=%llu", names[i], (unsigned long long)worst[i]);
    puts(line);
    used = snprintf(line, sizeof line, "XU4_COST ms=%lu", (unsigned long)(now / 1000));
    for (unsigned i = 0; i < XPROF_GAME_FIRST; ++i)
        used += snprintf(line + used, sizeof line - used,
                         " %s_us=%llu", names[i], (unsigned long long)totals[i]);
    puts(line);
    printf("XU4_WAIT ms=%lu action_n=%u action_req_us=%llu action_us=%llu action_audio_us=%llu action_max_us=%llu wait_frames=%u loop_n=%u loop_req_us=%llu loop_us=%llu loop_audio_us=%llu loop_max_us=%llu loop_skips=%u\n",
           (unsigned long)(now / 1000), action_wait.calls,
           (unsigned long long)action_wait.requested, (unsigned long long)action_wait.actual,
           (unsigned long long)action_wait.audio, (unsigned long long)action_wait.maximum,
           action_frames, loop_wait.calls, (unsigned long long)loop_wait.requested,
           (unsigned long long)loop_wait.actual, (unsigned long long)loop_wait.audio,
           (unsigned long long)loop_wait.maximum, loop_skips);
    printf("XU4_AUDIO_COST ms=%lu calls=%u empty=%u blocks=%u blocks_max=%u call_us=%llu call_max_us=%llu render_us=%llu pcm_read_us=%llu mix_us=%llu submit_us=%llu slow_blocks=%u slow_render_us=%llu slow_pcm_read_us=%llu slow_mix_us=%llu slow_submit_us=%llu\n",
        (unsigned long)(now/1000), audio_calls.calls, audio_calls.empty, audio_calls.blocks, audio_calls.blocks_max,
        (unsigned long long)audio_calls.total, (unsigned long long)audio_calls.maximum,
        (unsigned long long)audio_calls.parts[0], (unsigned long long)audio_calls.parts[1],
        (unsigned long long)audio_calls.parts[2], (unsigned long long)audio_calls.parts[3], audio_calls.slow_blocks,
        (unsigned long long)audio_calls.slow_parts[0], (unsigned long long)audio_calls.slow_parts[1],
        (unsigned long long)audio_calls.slow_parts[2], (unsigned long long)audio_calls.slow_parts[3]);
    printf("XU4_SCHED ms=%lu action_over_us=%llu action_over_max_us=%llu loop_over_us=%llu loop_over_max_us=%llu present_skips=%u late_skips=%u skip_age_start_max_ms=%u\n",
        (unsigned long)(now/1000), (unsigned long long)action_wait.over, (unsigned long long)action_wait.over_max,
        (unsigned long long)loop_wait.over, (unsigned long long)loop_wait.over_max,
        present_skips, late_skips, skip_age_max);
    printf("XU4_AUDIO_SCHED ms=%lu deadline_yields=%u budget_yields=%u low_blocks=%u reserve_frames=1764 guard_ms=6 block_budget=2 time_budget_us=6000\n",
        (unsigned long)(now/1000), audio_deadline_yields, audio_budget_yields, audio_low_blocks);
    audio_deadline_yields = audio_budget_yields = audio_low_blocks = 0;
    printf("XU4_AUDIO_RECOVERY ms=%lu active_calls=%u low_start_calls=%u queue_start_min=%u block_max_us=%llu video_presents=%u video_us=%llu emergency_defers=%u video_reserve_frames=882 audio_time_excludes_video=1\n",
        (unsigned long)(now/1000), recovery_calls, recovery_low_calls,
        recovery_calls ? recovery_queue_min : 0, (unsigned long long)recovery_block_max,
        recovery_presents, (unsigned long long)recovery_video_us, recovery_defers);
    recovery_calls = recovery_low_calls = recovery_presents = recovery_defers = 0;
    recovery_queue_min = UINT32_MAX;
    recovery_block_max = recovery_video_us = 0;
    // New categories are separate lines to keep existing bounded buffers safe.
    for (unsigned kind = 0; kind < 3; ++kind) {
        used = snprintf(line, sizeof line, "XU4_GAME_%s ms=%lu at_ms=%lu",
            kind == 0 ? "COST" : kind == 1 ? "SLOW" : "CALL",
            (unsigned long)(now/1000), (unsigned long)(worst_at/1000));
        for (unsigned i = XPROF_GAME_FIRST; i < XPROF_TILE_FIRST; ++i) {
            if (kind == 2)
                used += snprintf(line+used, sizeof line-used, " %s_n=%u %s_max_us=%llu",
                    names[i], game_calls[i], names[i], (unsigned long long)game_max[i]);
            else
                used += snprintf(line+used, sizeof line-used, " %s_us=%llu",
                    names[i], (unsigned long long)(kind == 0 ? totals[i] : worst[i]));
        }
        puts(line);
    }
    for (unsigned kind=0;kind<3;++kind) {
        used=snprintf(line,sizeof line,"XU4_TILE_%s ms=%lu at_ms=%lu",
            kind==0?"COST":kind==1?"SLOW":"CALL",(unsigned long)(now/1000),(unsigned long)(worst_at/1000));
        for(unsigned i=XPROF_TILE_FIRST;i<XPROF_COUNT;++i) {
            if(kind==2) used+=snprintf(line+used,sizeof line-used," %s_n=%u %s_max_us=%llu",names[i],game_calls[i],names[i],(unsigned long long)game_max[i]);
            else used+=snprintf(line+used,sizeof line-used," %s_us=%llu",names[i],(unsigned long long)(kind==0?totals[i]:worst[i]));
        }
        puts(line);
    }
    xu4_draw_report(now,worst_at);
    printf("XU4_BLIT ms=%lu opaque_pixels=%llu clear_pixels=%llu partial_pixels=%llu copy_pixels=%llu alpha_fast=1\n",
        (unsigned long)(now/1000),(unsigned long long)blit_opaque,(unsigned long long)blit_clear,
        (unsigned long long)blit_partial,(unsigned long long)blit_copy);
    printf("XU4_ROWCOPY ms=%lu fast_calls=%llu fast_rows=%llu fast_pixels=%llu fallback_calls=%llu fallback_pixels=%llu overlap_policy=conservative_span\n",
      (unsigned long)(now/1000),(unsigned long long)row_fast_calls,(unsigned long long)row_fast_rows,
      (unsigned long long)row_fast_pixels,(unsigned long long)row_fallback_calls,(unsigned long long)row_fallback_pixels);
    row_fast_calls=row_fast_rows=row_fast_pixels=row_fallback_calls=row_fallback_pixels=0;
    blit_opaque=blit_clear=blit_partial=blit_copy=0;
    memset(game_calls, 0, sizeof game_calls);
    memset(game_max, 0, sizeof game_max);
    audio_calls = {};
    present_skips = late_skips = skip_age_max = 0;
    action_wait = {};
    loop_wait = {};
    action_frames = loop_skips = 0;
    memset(totals, 0, sizeof totals);
    memset(worst, 0, sizeof worst);
    worst_gap = worst_at = 0;
    over100 = over500 = 0;
    window_keys = max_keys = worst_keys = worst_sleeps = 0;
}

#include "xu4_draw_trace.h"
struct DrawTraceStats { unsigned draws=0, sources[5]{}, pairs=0,same=0,scene=0,cycle=0,tiles=0,unknown=0; uint64_t parts[5]{}; };
static DrawTraceStats draw_win,draw_frame,draw_slow;
static Xu4DrawState draw_previous;
static bool draw_have_previous;
static unsigned draw_origin,draw_intervals,draw_multi,draw_max;
unsigned xu4_draw_origin_get(){return draw_origin;}
void xu4_draw_origin_set(unsigned value){draw_origin=value;}
void xu4_draw_complete(const Xu4DrawState& state,unsigned origin,const uint64_t* parts){
 DrawTraceStats delta;delta.draws=1;delta.sources[origin<5?origin:0]=1;
 for(unsigned i=0;i<5;++i)delta.parts[i]=parts[i];
 if(draw_have_previous){
  delta.pairs=1;
  const auto& p=draw_previous;
  bool scene=state.map!=p.map||state.x!=p.x||state.y!=p.y||state.z!=p.z||state.view!=p.view||state.flags!=p.flags;
  bool cycle=state.cycle!=p.cycle;
  bool known=state.software&&p.software;
  bool tiles=known&&state.tiles!=p.tiles;
  delta.scene=scene;delta.cycle=cycle;delta.tiles=tiles;delta.unknown=!known;
  delta.same=known&&!scene&&!cycle&&!tiles;
 }
 draw_previous=state;draw_have_previous=true;
 for(auto* t:{&draw_win,&draw_frame}){
  t->draws+=delta.draws;t->pairs+=delta.pairs;t->same+=delta.same;t->scene+=delta.scene;t->cycle+=delta.cycle;t->tiles+=delta.tiles;t->unknown+=delta.unknown;
  for(unsigned i=0;i<5;++i){t->sources[i]+=delta.sources[i];t->parts[i]+=delta.parts[i];}
 }
}
void xu4_draw_frame(bool slowest,bool valid){
 if(!valid){draw_win={};draw_frame={};draw_have_previous=false;return;}
 ++draw_intervals;if(draw_frame.draws>1)++draw_multi;
 if(draw_frame.draws>draw_max)draw_max=draw_frame.draws;
 if(slowest)draw_slow=draw_frame;
 draw_frame={};draw_have_previous=false;
}
static void draw_line(const char* tag,uint64_t now,uint64_t at,const DrawTraceStats& s){
 printf("XU4_DRAW_%s ms=%lu at_ms=%lu draws=%u other=%u turn=%u timer=%u key=%u callback=%u pairs=%u same_observed=%u scene_changed=%u cycle_changed=%u tiles_changed=%u unknown_pairs=%u setup_us=%llu collect_us=%llu los_us=%llu tiles_us=%llu tail_us=%llu\n",
 tag,(unsigned long)(now/1000),(unsigned long)(at/1000),s.draws,s.sources[0],s.sources[1],s.sources[2],s.sources[3],s.sources[4],s.pairs,s.same,s.scene,s.cycle,s.tiles,s.unknown,
 (unsigned long long)s.parts[0],(unsigned long long)s.parts[1],(unsigned long long)s.parts[2],(unsigned long long)s.parts[3],(unsigned long long)s.parts[4]);
}
void xu4_draw_report(uint64_t now,uint64_t at){
 draw_line("COST",now,at,draw_win);draw_line("SLOW",now,at,draw_slow);
 printf("XU4_DRAW_INTERVAL ms=%lu intervals=%u multi_draw=%u draws_max=%u state_scope=map_coords_view_flags_cycle_tile_id_frame_los hash=fnv32\n",(unsigned long)(now/1000),draw_intervals,draw_multi,draw_max);
 draw_win={};draw_slow={};draw_intervals=draw_multi=draw_max=0;
}

extern "C" bool xu4_uart_trace_room(){
 if(!uart_queued)return false;
 critical_section_enter_blocking(&uart_lock);
 const bool room=uart_queue.used<512;
 critical_section_exit(&uart_lock);
 return room;
}
