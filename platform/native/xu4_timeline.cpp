#include "xu4_timeline.h"
#include "xu4_profile.h"
#include "xu4_draw_trace.h"
#include "xu4_tail_diag.h"
#include <cstdio>
#include <cstring>
#include <climits>
namespace {
constexpr unsigned CAP=96, BLOCK=100;
struct Event {uint32_t start,us,context;int32_t video,wait;uint16_t kind,flags,q0,q1,origin,pad;};
struct Trace {uint64_t start,end,report;int32_t due;unsigned n,lost;Event events[CAP];};
struct Active {uint64_t start;unsigned context,origin;int64_t video,wait;bool running,carry;};
static Active active[XPROF_COUNT];
static Trace current,best,pending;
static uint64_t skipped_windows;
static unsigned cursor;
static bool sending;
static int32_t narrow(int64_t v){return v>INT32_MAX?INT32_MAX:v<INT32_MIN?INT32_MIN:int32_t(v);}
static uint32_t small(uint64_t v){return v>UINT32_MAX?UINT32_MAX:uint32_t(v);}
static bool selected(unsigned c){return c==XPROF_INPUT||c==XPROF_TICK||c==XPROF_AUDIO||c==XPROF_TRACK||c==XPROF_DECODE||c==XPROF_COPY||c==XPROF_SLEEP||c==XPROF_DRAW||c==XPROF_TIMER||c==XPROF_MOVE;}
static const char *name(unsigned c){switch(c){case XPROF_INPUT:return "input";case XPROF_TICK:return "tick";case XPROF_AUDIO:return "audio";case XPROF_TRACK:return "track";case XPROF_DECODE:return "decode";case XPROF_COPY:return "copy";case XPROF_SLEEP:return "wait";case XPROF_DRAW:return "draw";case XPROF_TIMER:return "timer";case XPROF_MOVE:return "move";default:return "block";}}
static void append(unsigned kind,uint64_t start,uint64_t end,unsigned flags,unsigned context,unsigned origin,int64_t video,int64_t wait,unsigned q0=0,unsigned q1=0){
 if(!current.start||end<start)return;
 if(start<current.start){start=current.start;flags|=1;}
 // Suppress empty service calls, but never omit frame-crossing scopes.
 if(kind==XPROF_AUDIO && !flags && end-start<250)return;
 if(current.n==CAP){++current.lost;return;}
 current.events[current.n++]={small(start-current.start),small(end-start),context,narrow(video),narrow(wait),uint16_t(kind),uint16_t(flags),uint16_t(q0),uint16_t(q1),uint16_t(origin),0};
}
}
void xu4_timeline_begin(unsigned c,uint64_t now){
 if(!selected(c))return;
 active[c]={now,xu4_timeline_context(),xu4_draw_origin_get(),xu4_video_left_us(now),xu4_diag_wait_left(now),true,false};
}
void xu4_timeline_end(unsigned c,uint64_t now){
 if(!selected(c)||!active[c].running)return;
 auto &a=active[c];append(c,a.start,now,a.carry?1:0,a.context,a.origin,a.video,a.wait);a.running=false;
}
void xu4_timeline_block(uint64_t start,uint64_t end,unsigned q0,unsigned q1,int64_t video,int64_t wait){
 append(BLOCK,start,end,0,xu4_timeline_context(),0,video,wait,q0,q1);
}
void xu4_timeline_frame(uint64_t now){
 for(unsigned c=0;c<XPROF_COUNT;++c){auto &a=active[c];if(!a.running)continue;
  append(c,a.start,now,2|(a.carry?1:0),a.context,a.origin,a.video,a.wait);
  a.start=now;a.carry=true;a.context=xu4_timeline_context();a.video=xu4_video_left_us(now);a.wait=xu4_diag_wait_left(now);
 }
 current.end=now;
 if(current.start && now-current.start>best.end-best.start)best=current;
 current.start=now;current.end=now;current.n=current.lost=0;current.due=narrow(xu4_video_left_us(now));
}
void xu4_timeline_window(uint64_t now){
 if(best.end-best.start>=60000){
  if(sending)++skipped_windows;
  else{pending=best;pending.report=now;cursor=0;sending=true;}
 }
 best={};
}
void xu4_timeline_drain(){
 if(!sending||!xu4_uart_trace_room())return;
 if(cursor==0){printf("XU4_TIMELINE report_ms=%llu start_us=%llu end_us=%llu gap_us=%llu due_us=%ld n=%u lost=%u skipped=%llu\n",pending.report/1000,pending.start,pending.end,pending.end-pending.start,(long)pending.due,pending.n,pending.lost,skipped_windows);}
 else if(cursor<=pending.n){const auto &e=pending.events[cursor-1];printf("XU4_TL end_us=%llu i=%u kind=%s start=%lu us=%lu flags=%u ctx=%lu origin=%u v=%ld w=%ld q0=%u q1=%u\n",pending.end,cursor,name(e.kind),(unsigned long)e.start,(unsigned long)e.us,e.flags,(unsigned long)e.context,e.origin,(long)e.video,(long)e.wait,e.q0,e.q1);}
 else{printf("XU4_TL_END end_us=%llu n=%u\n",pending.end,pending.n);sending=false;}
 ++cursor;
}
