#include "xu4_tail_diag.h"
#include "pico/stdlib.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
static Xu4TailWait *active;
static uint64_t wait_yields[3];
void xu4_diag_wait_yield(unsigned kind){if(kind<3)++wait_yields[kind];}
static uint64_t block_count,low_count,video_due_count,wait_due_count;
static unsigned queue_min=UINT32_MAX,queue_after_min=UINT32_MAX;
static uint64_t call_max,call_at;static unsigned call_n;static Xu4BlockTrace worst[10];
struct TailResult {uint64_t count,over_total,max,at,requested,actual,stages[TAIL_COUNT],late_starts;};
static TailResult tail[2];
Xu4TailWait::Xu4TailWait(uint64_t d,bool a):parent(active),deadline(d),start(time_us_64()),action(a){active=this;}
Xu4TailWait::~Xu4TailWait(){
 const auto end=time_us_64();const uint64_t excess=end>deadline?end-deadline:0;auto &r=tail[action?1:0];
 ++r.count;r.over_total+=excess;
 if(excess>r.max){r.max=excess;r.at=end;r.requested=deadline>start?deadline-start:0;r.actual=end-start;memcpy(r.stages,over,sizeof over);r.late_starts=late_starts;}
 active=parent;
}
int64_t xu4_diag_wait_left(uint64_t now){return active?int64_t(active->deadline)-int64_t(now):INT64_MAX;}
void xu4_diag_stage(unsigned stage,uint64_t start,uint64_t end){
 if(!active||stage>=TAIL_COUNT)return;
 const auto cut=std::max(start,active->deadline);
 if(end>cut)active->over[stage]+=end-cut;
 if(start>=active->deadline)++active->late_starts;
}
void xu4_diag_audio(uint64_t at,uint64_t us,unsigned count,const Xu4BlockTrace *blocks){
 if(!count)return;
 block_count+=count;
 for(unsigned i=0;i<count&&i<10;++i){const auto &b=blocks[i];queue_min=std::min(queue_min,b.before);queue_after_min=std::min(queue_after_min,b.after);low_count+=b.before<1764;video_due_count+=b.video_before<=0;wait_due_count+=b.wait_before<=0;}
 if(us>call_max){call_max=us;call_at=at;call_n=std::min(count,10u);memcpy(worst,blocks,call_n*sizeof(*blocks));}
}
void xu4_diag_report(uint64_t now){
 printf("XU4_WAIT_YIELD ms=%lu scheduler_skip=%llu block_stop=%llu low_continue=%llu reserve_frames=1764\n",(unsigned long)(now/1000),wait_yields[0],wait_yields[1],wait_yields[2]);
 memset(wait_yields,0,sizeof wait_yields);
 printf("XU4_BLOCK_WIN ms=%lu blocks=%llu low_start=%llu video_due_start=%llu wait_due_start=%llu q_min=%u q_after_min=%u call_at_ms=%llu call_max_us=%llu trace_n=%u\n",(unsigned long)(now/1000),block_count,low_count,video_due_count,wait_due_count,queue_min,queue_after_min,call_at/1000,call_max,call_n);
 for(unsigned i=0;i<call_n;++i){const auto &b=worst[i];printf("XU4_BLOCK_TRACE ms=%lu i=%u q0=%u q1=%u us=%llu v0=%lld v1=%lld w0=%lld w1=%lld\n",(unsigned long)(now/1000),i+1,b.before,b.after,b.us,b.video_before,b.video_after,b.wait_before,b.wait_after);}
 for(unsigned i=0;i<2;++i){const auto &r=tail[i];printf("XU4_WAIT_TAIL ms=%lu action=%u n=%llu over_total_us=%llu max_us=%llu at_ms=%llu req_us=%llu actual_us=%llu usb=%llu audio=%llu video=%llu uart=%llu sleep=%llu late_stages=%llu\n",(unsigned long)(now/1000),i,r.count,r.over_total,r.max,r.at/1000,r.requested,r.actual,r.stages[0],r.stages[1],r.stages[2],r.stages[3],r.stages[4],r.late_starts);}
 block_count=low_count=video_due_count=wait_due_count=call_max=call_at=0;call_n=0;queue_min=queue_after_min=UINT32_MAX;memset(tail,0,sizeof tail);
}
