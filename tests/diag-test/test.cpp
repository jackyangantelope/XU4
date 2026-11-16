#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <string>
#include <vector>
#include "pico/stdio_uart.h"
static uint64_t test_time=1000000;
uint64_t time_us_64(){return test_time;}
void sleep_us(unsigned us){test_time+=us;}
static std::string wire;
static unsigned blocking_writes,blocking_flushes,slots;
static void direct(const char *p,int n){++blocking_writes;wire.append(p,n);}
static void flush(){++blocking_flushes;}
stdio_driver_t stdio_uart={direct,flush};
bool uart_is_writable(int){return slots>0;}
void uart_putc_raw(int,char c){assert(slots);--slots;wire+=c;}
int routed_printf(const char *fmt,...){char b[4096];va_list a;va_start(a,fmt);int n=vsnprintf(b,sizeof b,fmt,a);va_end(a);assert(n>=0&&n<int(sizeof b));stdio_uart.out_chars(b,n);stdio_uart.out_flush();return n;}
int routed_puts(const char *p){return routed_printf("%s\n",p);}
#define printf routed_printf
#define puts routed_puts
#include "xu4_profile.cpp"
#define worst diag_worst
#include "xu4_tail_diag.cpp"
#undef worst
#define main firmware_main
#include "board_hal.cpp"
#undef main
#undef printf
#undef puts
static uint16_t framebuffer[320*240];static uint32_t pixels[320*200];
uint16_t *jack_wave1_framebuffer(){return framebuffer;}
void jack_wave1_audio_report(){routed_puts("AUDIO mock");}
#define printf routed_printf
#include "xu4_fxdecode_diag.h"
#undef printf
static Xu4FxDecodeDiag test_decode;
#define printf routed_printf
#include "xu4_track_diag.h"
#undef printf
static Xu4TrackDiag track_fixture;
extern "C" void xu4_boot_finish(){}
#define printf routed_printf
namespace read_fixture {
#include "xu4_read_diag.c"
}
#undef printf
extern "C" void xu4_integrity_finish(){}
extern "C" void xu4_report_footstep_cache(uint64_t now){

 using namespace read_fixture;
 if(!count){xu4_read_begin(2,"towne_2.it");for(unsigned i=0;i<5;++i)xu4_read_record(i?65536:1,32768,0,999999,99999999);xu4_read_open(0,99999,9999999);xu4_read_seek(0,99999,100000,0);xu4_read_end(0);}
 xu4_read_report(now);

 Xu4TrackDiag::Event e;e.start_us=now-2000000;e.id=2;e.total_us=1026000;e.load_us=1025000;e.used_before=5000000;e.used_stopped=4000000;e.used_loaded=6000000;e.used_after=7000000;e.arena_before=e.arena_after=8000000;for(int i=0;i<127;++i)e.file[i]='x';track_fixture.record(e);e.start_us+=1;e.total_us+=1;track_fixture.record(e);track_fixture.report(now);
routed_puts("XU4_TITLE_PCM ms=9999999 id=0 file=title_fade.ogg shared=1 opens=9 active=1 buffer_bytes=16384 reads=9999 read_us=999999999 read_max_us=9999999 open_max_us=9999999 demand_reads=999 errors=999 min_ahead_frames=99999");std::string longpath(140,'x');test_decode.record(5,longpath.c_str(),now-1024000,1024000,999999);test_decode.record(6,longpath.c_str(),now-1000,1000,-1);test_decode.report(now);routed_puts("XU4_FXCACHE ms=9999999 mode=ON hits=999 decodes=9 decode_max_us=9999999 pcm_bytes=524288 blocked_hits=999 blocked_decodes=9 blocked_decode_max_us=9999999");routed_puts("XU4_GATE_PCM ms=9999999 id=27 file=gate_open.ogg shared=1 opens=9 active=1 buffer_bytes=16384 reads=9999 read_us=999999999 read_max_us=9999999 open_max_us=9999999 demand_reads=999 errors=999 min_ahead_frames=99999");}
size_t occurrences(const char *s){size_t n=0,p=0;while((p=wire.find(s,p))!=std::string::npos){++n;p+=std::string(s).size();}return n;}
extern "C" void xu4_schedule_report(uint64_t){routed_puts("XU4_PRESENT_SCHED ms=9999999 period_us=41666 missed_slots=999 late_max_us=9999999 due_before_audio=999 due_after_audio=999 boundary=complete_wait audio_nested_present=0 pre_checks=9999 pre_due=999 pre_near=999 pre_frames=999 pre_max_us=9999999 exit_checks=999 exit_due=999 exit_frames=999 exit_max_us=9999999");}
extern "C" void xu4_schedule_service(){}
bool xu4_in_action_wait(){return false;}
unsigned xu4_timeline_context(){return 65537;}
int64_t xu4_video_left_us(uint64_t){return 30000;}
static bool key_pending;static jack_wave1_key_event_t test_key;
void jack_wave1_usb_service(){}
bool jack_wave1_usb_pop_key(jack_wave1_key_event_t *key){if(!key_pending)return false;*key=test_key;key_pending=false;return true;}
uint8_t jack_wave1_usb_current_modifiers(){return 0;}
extern "C" bool native_usb_key_held(uint8_t){return false;}
int main(){
 {const auto start=test_time;Xu4TailWait w(start+1000,true);assert(xu4_diag_wait_left(start)==1000);
  xu4_diag_stage(TAIL_AUDIO,start+900,start+1400);test_time=start+1500;
  assert(w.over[TAIL_AUDIO]==400);{Xu4TailWait inner(test_time+100,false);test_time+=120;}
  assert(active==&w);test_time=start+2000;}
 assert(active==nullptr&&tail[1].max==1000&&tail[1].stages[TAIL_AUDIO]==400&&tail[0].max==20);
 memset(tail,0,sizeof tail);
 Xu4BlockTrace blocks[10];for(unsigned i=0;i<10;++i)blocks[i]={i*100,i*100+200,6000,3000-int64_t(i)*6000,-3000-int64_t(i)*6000,INT64_MAX,INT64_MAX};
 xu4_diag_audio(test_time,60000,10,blocks);assert(block_count==10&&low_count==10&&video_due_count==9&&wait_due_count==0&&call_n==10);
 {Xu4TailWait live(test_time+1000,true);xu4_diag_report(test_time);assert(active==&live&&call_n==0&&block_count==0);}
 memset(tail,0,sizeof tail);

 assert(!compare_enabled&&!compare_requested);
 static uint32_t src[320*200];static uint16_t ref[320*240],cmp[320*240];
 unsigned rng=7;
 for(int trial=0;trial<201;++trial){
  for(auto &x:src){rng=rng*1664525u+1013904223u;x=rng;}
  for(unsigned i=0;i<320*240;++i){rng=rng*1664525u+1013904223u;ref[i]=cmp[i]=uint16_t(rng);}
  const int off=trial%200;unsigned expected=0;
  for(unsigned y=0;y<200;++y)for(unsigned x=0;x<320;++x){
   const unsigned char *s=(const unsigned char*)src+(y>=unsigned(off)?(y-off)*1280:0);
   uint16_t value=y<unsigned(off)?0:uint16_t((s[x*4]>>3)<<10|(s[x*4+1]>>3)<<5|(s[x*4+2]>>3));
   expected+=cmp[(y+20)*320+x]!=value;
  }
  assert(copy_frame<false>(src,ref,off)==0);assert(copy_frame<true>(src,cmp,off)==expected);
  assert(memcmp(ref,cmp,sizeof ref)==0);
 }
 uint8_t usage=0,mod=0;
 test_key={65,0};key_pending=true;assert(native_key(&usage,&mod)==1&&usage==65&&mod==0);
 assert(!compare_requested); // Plain F8 retains game mapping
 test_key={65,5};key_pending=true;assert(native_key(&usage,&mod)==0);
 assert(compare_requested&&!compare_enabled&&repeat_key==0);
 assert(native_key(&usage,&mod)==0&&compare_requested); // no autorepeat toggle

 compare_enabled=true; // Later production native_present report tests deferred application.

 xu4_prof_rowcopy(1,16,256);xu4_prof_rowcopy(0,8,64);assert(row_fast_calls==1&&row_fast_rows==16&&row_fast_pixels==256&&row_fallback_calls==1&&row_fallback_pixels==64);
 xu4_prof_frame(test_time);
 xu4_prof_enter(XPROF_TILE_LAYER);test_time+=100;
 xu4_prof_enter(XPROF_TILE_ANIM);test_time+=200;
 xu4_prof_enter(XPROF_TILE_COPY);test_time+=300;
 xu4_prof_frame(test_time);test_time+=400;
 xu4_prof_leave(XPROF_TILE_COPY);test_time+=500;
 xu4_prof_leave(XPROF_TILE_ANIM);test_time+=600;
 xu4_prof_leave(XPROF_TILE_LAYER);xu4_prof_frame(test_time);
 assert(totals[XPROF_TILE_COPY]==700&&totals[XPROF_TILE_ANIM]==1400&&totals[XPROF_TILE_LAYER]==2100);
 assert(game_calls[XPROF_TILE_COPY]==1&&game_max[XPROF_TILE_COPY]==700);
 assert(worst[XPROF_TILE_COPY]==400);

 xu4_prof_blit(2,3,4,5);assert(blit_opaque==2&&blit_clear==3&&blit_partial==4&&blit_copy==5);
 {Xu4DrawOrigin o(2);assert(xu4_draw_origin_get()==2);{Xu4DrawOrigin f(4,true);assert(xu4_draw_origin_get()==2);}{Xu4DrawOrigin t(1);assert(xu4_draw_origin_get()==1);}assert(xu4_draw_origin_get()==2);}assert(xu4_draw_origin_get()==0);
 Xu4DrawState ds{1,1,2,0,0,4,1,123,true};uint64_t dp[5]={1,2,3,4,5};
 xu4_draw_complete(ds,1,dp);xu4_draw_complete(ds,2,dp);
 assert(draw_frame.draws==2&&draw_frame.pairs==1&&draw_frame.same==1);
 ds.cycle++;xu4_draw_complete(ds,2,dp);ds.x++;ds.tiles++;xu4_draw_complete(ds,3,dp);
 assert(draw_frame.cycle==1&&draw_frame.scene==1&&draw_frame.tiles==1);
 xu4_draw_frame(true);assert(draw_slow.draws==4&&draw_max==4&&draw_multi==1);
 xu4_draw_complete(ds,0,dp);assert(draw_frame.pairs==0);
 ds.software=false;xu4_draw_complete(ds,0,dp);assert(draw_frame.unknown==1&&draw_frame.same==0);
 draw_win={};draw_frame={};draw_slow={};draw_intervals=draw_multi=draw_max=0;draw_have_previous=false;

 {Xu4DrawOrigin origin(2);Xu4DrawTrace trace(1,2,3,0,0,7,1);trace.software();trace.tile(42);test_time+=10;trace.phase(0);test_time+=20;trace.phase(1);test_time+=30;trace.phase(2);test_time+=40;trace.phase(3);test_time+=50;}
 assert(draw_frame.draws==1&&draw_frame.sources[2]==1);
 for(unsigned i=0;i<5;++i)assert(draw_frame.parts[i]==10*(i+1));
 draw_win={};draw_frame={};draw_slow={};draw_intervals=draw_multi=draw_max=0;draw_have_previous=false;
 // Cross-frame and nested timing: frame totals split, whole-call max retained.
 xu4_prof_frame(test_time);
 xu4_prof_enter(XPROF_TURN);test_time+=1000;
 xu4_prof_enter(XPROF_DRAW);test_time+=2000;
 xu4_prof_frame(test_time);
 assert(totals[XPROF_TURN]==3000 && totals[XPROF_DRAW]==2000);
 xu4_prof_enter(XPROF_DRAW);test_time+=500;xu4_prof_leave(XPROF_DRAW);
 test_time+=500;xu4_prof_leave(XPROF_DRAW);
 test_time+=1000;xu4_prof_leave(XPROF_TURN);xu4_prof_frame(test_time);
 assert(totals[XPROF_TURN]==5000 && totals[XPROF_DRAW]==3000);
 assert(game_calls[XPROF_DRAW]==1&&game_max[XPROF_DRAW]==3000);
 assert(game_calls[XPROF_TURN]==1&&game_max[XPROF_TURN]==5000);

 xu4_prof_audio_recovery(1200,3,7000,1,9500,2);
 xu4_prof_audio_recovery(2800,1,3000,0,0,0);
 assert(recovery_defers==2&&recovery_calls==2&&recovery_low_calls==1&&recovery_queue_min==1200&&recovery_block_max==7000&&recovery_presents==1&&recovery_video_us==9500);
 xu4_prof_audio_yield(1);xu4_prof_audio_yield(2);xu4_prof_audio_yield(3);
 assert(audio_deadline_yields==1&&audio_budget_yields==1&&audio_low_blocks==1);
 const uint64_t p1[4]={200,100,300,50},p2[4]={700,200,400,100};
 xu4_prof_audio_call(800,2,p1);xu4_prof_audio_call(1500,4,p2);xu4_prof_audio_call(10,0,p1);
 assert(audio_calls.calls==3&&audio_calls.empty==1&&audio_calls.blocks==6&&audio_calls.maximum==1500&&audio_calls.slow_blocks==4&&audio_calls.slow_parts[0]==700);
 {Xu4WaitScope w(true,41);test_time+=74000;}
 assert(action_wait.over==33000&&action_wait.over_max==33000);
 {Xu4WaitScope w(false,10);test_time+=9000;}
 assert(loop_wait.over==0);
 xu4_prof_skip_age(20,41);xu4_prof_skip_age(41,41);xu4_prof_skip_age(70,41);
 assert(present_skips==3&&late_skips==2&&skip_age_max==70);
 // Queue wrap, strict per-service budget, no writes to a full FIFO, exact byte order.
 Xu4UartQueue q;std::string expected,got;
 for(int k=0;k<200;++k){std::string s(133,char(k));q.push(s.data(),s.size());expected+=s;
   while(q.used){unsigned n=q.drain([]{return true;},[&](char c){got+=c;});assert(n<=16);}}
 assert(got==expected && q.dropped==0);
 std::vector<char> full(q.capacity,'x');q.push(full.data(),full.size());q.push("bad",3);
 assert(q.used==q.capacity&&q.dropped==3&&q.peak==q.capacity);
 assert(q.drain([]{return false;},[](char){assert(false);})==0);
 blocking_writes=blocking_flushes=0;wire.clear();
 xu4_prof_uart_init();xu4_prof_uart_init();
 routed_puts("startup");assert(blocking_writes==1);
 native_present(pixels,320,200,0);
 assert(fresh_frames==0&&repeat_frames==0);
 native_present(pixels,320,200,0);assert(repeat_frames==1&&fresh_frames==0);
 pixels[0]=255;native_present(pixels,320,200,0);assert(fresh_frames==1&&changed_pixels==1);
 pixels[0]=254;native_present(pixels,320,200,0);assert(repeat_frames==2); // Same RGB555
 native_present(pixels,320,200,0); // production HAL enables queue after first frame
 xu4_prof_report(test_time);
 assert(audio_calls.calls==0&&action_wait.over==0&&present_skips==0);
 assert(game_calls[XPROF_DRAW]==0&&game_max[XPROF_TURN]==0);
 assert(game_calls[XPROF_TILE_COPY]==0&&game_max[XPROF_TILE_COPY]==0);
 assert(draw_win.draws==0&&draw_intervals==0);
 assert(blit_opaque==0&&blit_clear==0&&blit_partial==0&&blit_copy==0);
 assert(row_fast_calls==0&&row_fast_rows==0&&row_fast_pixels==0&&row_fallback_calls==0&&row_fallback_pixels==0);
 assert(recovery_defers==0&&recovery_calls==0&&recovery_queue_min==UINT32_MAX&&recovery_block_max==0&&recovery_video_us==0);
 assert(audio_deadline_yields==0&&audio_budget_yields==0&&audio_low_blocks==0);
 while(uart_queue.used){slots=32;xu4_uart_service();}
 assert(wire.find("call_max_us=1500")!=std::string::npos&&wire.find("action_over_max_us=33000")!=std::string::npos&&wire.find("late_skips=2")!=std::string::npos);
 const unsigned before=blocking_writes,beforeFlush=blocking_flushes;
 wire.clear();slots=0;routed_puts("ordered A");routed_puts("ordered B");
 assert(wire.empty() && uart_queue.used>0);
 xu4_uart_service();assert(wire.empty());
 while(uart_queue.used){slots=32;auto n=wire.size();xu4_uart_service();assert(wire.size()-n<=16);}
 assert(wire=="ordered A\nordered B\n");
 wire.clear();
 // 130 seconds, service opportunities every 500us. Six FIFO slots/tick approximate
 // the 115200-baud throughput; the real HAL/profile code generates each report.
 size_t all_peak=0;
 for(int i=0;i<260000;++i){
 if(i==100000){request_compare_toggle();assert(compare_enabled&&!compare_requested);}
 if(i==120000){assert(!compare_enabled);request_compare_toggle();assert(!compare_enabled&&compare_requested);}
 if(i==140000)assert(compare_enabled);
 if(i%10000==0){xu4_diag_audio(test_time,60000,10,blocks);}
 // Fill worst-frame snapshots with the full 96-event capacity while preserving actual queue servicing.
 if(i%10000==5){for(unsigned k=0;k<110;++k)xu4_timeline_block(test_time,test_time+1,1764,2205,30000,40000);test_time+=65000;}
 test_time+=500;slots=6;xu4_uart_service();if(i%82==0){xu4_prof_key();native_present(pixels,320,200,0);}if(uart_queue.used>all_peak)all_peak=uart_queue.used;}
 while(uart_queue.used){slots=6;xu4_uart_service();}
 assert(occurrences("XU4_TIMELINE ")>=25);assert(occurrences("XU4_TL_END ")>=24);assert(occurrences("XU4_FPS ")>=25);
 assert(occurrences("XU4_READ_LOAD ")>=8 && occurrences("XU4_READ_BINS ")>=8);
 assert(occurrences("XU4_BLOCK_TRACE ")>=250);
 assert(occurrences("XU4_WAIT_TAIL ")>=50);
 assert(occurrences("XU4_WAIT_YIELD ")>=25);
 assert(wire.find("compare=OFF measured=0")!=std::string::npos);
 assert(wire.find("XU4_COMPARE active=ON")!=std::string::npos);
 assert(occurrences("XU4_UART ")==occurrences("XU4_FPS "));
 assert(occurrences("XU4_COST ")==occurrences("XU4_FPS "));
 assert(occurrences("XU4_AUDIO_COST ")==occurrences("XU4_FPS "));
 assert(occurrences("XU4_SCHED ")==occurrences("XU4_FPS "));
 assert(uart_queue.dropped==0 && uart_queue.peak<uart_queue.capacity);
 assert(blocking_writes==before&&blocking_flushes==beforeFlush);
 fprintf(stderr,"PASS: queue order/wrap/overflow/FIFO-full/budget; production HAL + profiler 130s continuous output, zero blocking writes/flushes, peak=%zu, dropped=0\n",all_peak);
}
