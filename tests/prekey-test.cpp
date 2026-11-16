#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include "xu4_profile.h"
#include "xu4_wait.h"
#include "xu4_tail_diag.h"
#include "xu4_present_clock.h"
#include "xu4_audio_budget.h"
static uint64_t now=1000000;static unsigned queued=2000,audio_cost=100;static std::string order;static std::vector<uint64_t> copies;
static int frame_ms=41;struct {void *screenImage=(void*)1;} xu4;
uint64_t time_us_64(){return now;}uint32_t native_millis(){return now/1000;}
size_t native_audio_queued(){return queued;}
static void present_complete_image();void screenSwapBuffers();
void xu4_native_audio_service(){order+='A';now+=audio_cost;queued=2000;}
void xu4_prof_action_frame(){} void xu4_prof_skip_age(uint32_t,unsigned){}
void xu4_diag_stage(unsigned,uint64_t,uint64_t){}void xu4_diag_wait_yield(unsigned){}
int64_t xu4_diag_wait_left(uint64_t){return INT64_MAX;}
Xu4WaitScope::Xu4WaitScope(bool,unsigned){} Xu4WaitScope::~Xu4WaitScope(){}
extern "C" void xu4_prof_enter(Xu4ProfileCategory){}extern "C" void xu4_prof_leave(Xu4ProfileCategory){}
static uint32_t last_swap_start;
static bool have_swap;
static unsigned action_frame_count, swap_count, safe_depth, action_depth;
static bool in_scheduler;
// Pre-input presents are counted for outer-loop suppression, not as action waits.
static unsigned prekey_frames;
static unsigned exit_checks,exit_due,exit_frames;
static uint64_t exit_max_us;
static unsigned prekey_checks,prekey_due,prekey_near,prekey_window_frames;
static uint64_t prekey_max_us;
static void prekey_present(bool complete){
    if(!complete || !have_swap || !xu4.screenImage || in_scheduler)return;
    ++prekey_checks;
    const uint64_t start=time_us_64();
    const int64_t left=xu4_video_left_us(start);
    // Same 6ms near-deadline horizon used by the unchanged audio budget.
    if(left>6000)return;
    if(left<=0)++prekey_due;else ++prekey_near;
    const unsigned before=swap_count;
    screenSwapBuffers(); // original safe boundary, audio thresholds and phase clock
    const unsigned count=swap_count-before;
    prekey_frames+=count;prekey_window_frames+=count;
    const uint64_t elapsed=time_us_64()-start;
    if(elapsed>prekey_max_us)prekey_max_us=elapsed;
}
static Xu4PresentClock present_clock;
unsigned xu4_timeline_context(){return (safe_depth>255?255:safe_depth) | ((action_depth>255?255:action_depth)<<8) | (unsigned(in_scheduler)<<16);}
void xu4_complete_begin(){++safe_depth;}
void xu4_complete_end(){--safe_depth;}
int64_t xu4_video_left_us(uint64_t now){return present_clock.started?int64_t(present_clock.deadline)-int64_t(now):INT64_MAX;}
bool xu4_in_action_wait(){return action_depth!=0;}
unsigned xu4_video_remaining_ms() { return present_clock.remaining(time_us_64()); }
// Complete scopes include the outer loop's post-render wait. They end before
// the next controller/timer may modify the image. No nested audio presentation.
static unsigned due_before_audio, due_after_audio;
static bool present_if_due() {
    if(!safe_depth || !xu4.screenImage || (have_swap && xu4_video_remaining_ms()))return false;
    present_clock.begin(time_us_64());
    last_swap_start=native_millis();have_swap=true;++swap_count;
    if(action_depth){++action_frame_count;xu4_prof_action_frame();}
    const uint64_t diag_start=time_us_64();
    present_complete_image();
    xu4_diag_stage(TAIL_VIDEO,diag_start,time_us_64());
    return true;
}
extern "C" void xu4_schedule_service() {
    if(in_scheduler)return;
    in_scheduler=true;
    // Preserve the existing 882-frame video safety margin. If already due and
    // PCM is available, don't delay video by generating another audio batch.
    const bool presented = native_audio_queued()>=882 && present_if_due();
    if(presented)++due_before_audio;
    // Re-evaluate after the required video copy: it may expire the wait and
    // consume PCM. Preserve the 1764-frame non-emergency audio threshold.
    const unsigned queued_after_video=native_audio_queued();
    const int64_t wait_left=xu4_diag_wait_left(time_us_64());
    if(xu4_wait_audio_can_yield(queued_after_video,wait_left)) {
        xu4_diag_wait_yield(0);
    } else {
        const uint64_t diag_start=time_us_64();
        xu4_native_audio_service();
        xu4_diag_stage(TAIL_AUDIO,diag_start,time_us_64());
    }
    if(!presented && present_if_due())++due_after_audio;
    in_scheduler=false;
}
// One check after the action wait returns, before its complete-image scope ends.
static void action_wait_exit_present(){
    if(!have_swap || !xu4.screenImage || !safe_depth || !action_depth || in_scheduler)return;
    ++exit_checks;
    const uint64_t start=time_us_64();
    if(xu4_video_left_us(start)>0)return; // no near-deadline wait here
    ++exit_due;
    const unsigned before=swap_count;
    screenSwapBuffers(); // unchanged audio safety thresholds and submission clock
    exit_frames+=swap_count-before; // original action_depth counts this inline frame
    const uint64_t elapsed=time_us_64()-start;
    if(elapsed>exit_max_us)exit_max_us=elapsed;
}
extern "C" void xu4_schedule_report(uint64_t now) {
    printf("XU4_PRESENT_SCHED ms=%lu period_us=%llu missed_slots=%llu late_max_us=%llu due_before_audio=%u due_after_audio=%u boundary=complete_wait audio_nested_present=0 pre_checks=%u pre_due=%u pre_near=%u pre_frames=%u pre_max_us=%llu exit_checks=%u exit_due=%u exit_frames=%u exit_max_us=%llu\n",
        (unsigned long)(now/1000),(unsigned long long)present_clock.period,
        (unsigned long long)present_clock.missed,(unsigned long long)present_clock.late_max,due_before_audio,due_after_audio,prekey_checks,prekey_due,prekey_near,prekey_window_frames,(unsigned long long)prekey_max_us,exit_checks,exit_due,exit_frames,(unsigned long long)exit_max_us);
    present_clock.missed=present_clock.late_max=0;
    due_before_audio=due_after_audio=0;
    exit_checks=exit_due=exit_frames=0;exit_max_us=0;
    prekey_checks=prekey_due=prekey_near=prekey_window_frames=0;prekey_max_us=0;
}
unsigned xu4_action_frame_count() { return action_frame_count+prekey_frames; }
bool xu4_outer_present_needed(unsigned before) {
    return before==action_frame_count+prekey_frames || !have_swap || !xu4_video_remaining_ms();
}
extern "C" void xu4_note_skipped_present() {
    if(have_swap)xu4_prof_skip_age(uint32_t(native_millis()-last_swap_start),unsigned(frame_ms));
}
void screenSwapBuffers(){
#ifdef XU4_PICO
    if(!xu4.screenImage)return;
    Xu4CompleteScope boundary;
    const unsigned before=swap_count;
    xu4_schedule_service();
    if(swap_count==before) {
        Xu4WaitScope wait_scope(false,xu4_video_remaining_ms());
        // Wait on the exact microsecond deadline, including a final service at
        // expiry. Millisecond rounding must not let this return without a frame.
        while(swap_count==before)xu4_wait_until_us(present_clock.deadline);
    }
#else
    present_complete_image();
#endif
}

static void present_complete_image(){assert(safe_depth>0);order+='V';copies.push_back(now);now+=9500;queued=queued>419?queued-419:0;}
extern "C" void xu4_wait_until_us(uint64_t deadline){unsigned loops=0;do{xu4_schedule_service();if(now>=deadline)break;now+=(deadline-now<500?deadline-now:500);assert(++loops<100);}while(true);}
using updateScreenCallback=void(*)();
struct Controller {bool notifyKeyPressed(int k){assert(!safe_depth&&!in_scheduler);order+=k==1?'K':'P';return true;}} controller;
struct EventHandler {void handleInputEvents(Controller*,updateScreenCallback);Controller *getController(){return &controller;}};
static bool key_pending=true,pad_pending=true;
int native_quit(){return 0;}void quitGame(){assert(false);}
int native_key(uint8_t *u,uint8_t *m){*u=1;*m=0;return key_pending;}
void xu4_prof_key(){}int hid_key(uint8_t,uint8_t){return 1;}
uint16_t native_pad(){return 0;}
struct {int update(uint16_t,uint32_t){return pad_pending?2:0;}} pad;
static void update(){assert(!safe_depth);order+='U';}
static void reset(int64_t left){now=1000000;queued=2000;audio_cost=100;have_swap=true;safe_depth=action_depth=swap_count=action_frame_count=prekey_frames=0;in_scheduler=false;prekey_checks=prekey_due=prekey_near=prekey_window_frames=0;prekey_max_us=0;present_clock={};present_clock.started=true;present_clock.period=41666;present_clock.deadline=now+left;xu4.screenImage=(void*)1;order.clear();copies.clear();}
void EventHandler::handleInputEvents(Controller *wait,updateScreenCallback update){
    XU4_PROFILE_SCOPE(XPROF_INPUT);
    xu4_native_audio_service();
#ifdef XU4_PICO
    // run() completes updateScreen before its loop; wait_msecs passes NULL.
    // End this boundary before polling/dispatching any next key or pad action.
    prekey_present(!wait && update!=nullptr);
#endif
    if(native_quit()){quitGame();return;}
    uint8_t usage,mod;
    // Leave remaining events queued so each outer loop can present a frame.
    if(native_key(&usage,&mod)){
        xu4_prof_key();
        int key=hid_key(usage,mod);Controller *con=wait?wait:getController();
        if(key&&con&&con->notifyKeyPressed(key)&&update)update();
    }
    int key=pad.update(native_pad(),native_millis());
    Controller *con=wait?wait:getController();
    if(key&&con&&con->notifyKeyPressed(key)&&update)update();
}


static int64_t sleep_exit_left=-1;
static unsigned sleep_calls;
void native_sleep(unsigned ms){++sleep_calls;now+=uint64_t(ms)*1000;present_clock.deadline=now+sleep_exit_left;if(xu4.screenImage)assert(safe_depth==1&&action_depth==1);}
void screenWait(int frames){
#ifdef XU4_PICO
    if (frames <= 0) return;
    const unsigned duration = unsigned(frames) * frame_ms;
    Xu4WaitScope wait_scope(true, duration);
    if (!xu4.screenImage) {
        native_sleep(duration);
        return;
    }
    Xu4CompleteScope boundary;
    ++action_depth;
    native_sleep(duration);
    action_wait_exit_present();
    --action_depth;
#else
    native_sleep(frames*frame_ms);
#endif
}

int main(){
 reset(6001);prekey_present(true);assert(copies.empty()&&prekey_checks==1);
 reset(6000);unsigned before=xu4_action_frame_count();uint64_t deadline=present_clock.deadline;prekey_present(true);assert(copies.size()==1&&copies[0]>=deadline&&prekey_near==1&&prekey_frames==1);assert(!xu4_outer_present_needed(before));now=present_clock.deadline;assert(xu4_outer_present_needed(before));
 reset(-50000);prekey_present(true);assert(copies.size()==1&&prekey_due==1&&present_clock.missed==1);
 reset(-1);queued=100;audio_cost=14000;prekey_present(true);assert(order[0]=='A'&&order.find('V')>0&&copies.size()==1);
 reset(-1);prekey_present(false);assert(order.empty());have_swap=false;prekey_present(true);assert(order.empty());have_swap=true;xu4.screenImage=nullptr;prekey_present(true);assert(order.empty());xu4.screenImage=(void*)1;in_scheduler=true;prekey_present(true);assert(order.empty());
 // Actual production input handler retains audio -> keyboard/update -> pad/update order;
 // deadline service ends before any key is dispatched, no event loss or duplicated callbacks.
 EventHandler eh;reset(5000);queued=100;audio_cost=14000;before=xu4_action_frame_count();eh.handleInputEvents(nullptr,update);assert(order.find('V')<order.find('K'));assert(order.substr(order.find('K'))=="KUPU");assert(prekey_frames==1&&!safe_depth&&!in_scheduler);assert(!xu4_outer_present_needed(before));
 reset(30000);eh.handleInputEvents(nullptr,update);assert(order=="AKUPU"&&copies.empty());
 reset(-100);eh.handleInputEvents(&controller,nullptr);assert(order=="AKP"&&copies.empty());
 reset(-100);eh.handleInputEvents(nullptr,nullptr);assert(order=="AKP"&&copies.empty());
 reset(6000);action_frame_count=UINT32_MAX;before=xu4_action_frame_count();prekey_present(true);assert(xu4_action_frame_count()==0&&!xu4_outer_present_needed(before));
 unsigned snapshot=prekey_frames;xu4_schedule_report(now);assert(prekey_frames==snapshot&&prekey_window_frames==0&&prekey_checks==0);
 // Production screenWait exit hook runs inside its original action/complete scope.
 reset(30000);exit_checks=exit_due=exit_frames=0;exit_max_us=0;sleep_exit_left=1;sleep_calls=0;
 screenWait(1);assert(sleep_calls==1&&exit_checks==1&&exit_due==0&&copies.empty()&&!safe_depth&&!action_depth);
 reset(30000);exit_checks=exit_due=exit_frames=0;sleep_exit_left=0;before=xu4_action_frame_count();
 screenWait(1);assert(exit_checks==1&&exit_due==1&&exit_frames==1&&copies.size()==1&&action_frame_count==1&&!safe_depth&&!action_depth);assert(!xu4_outer_present_needed(before));
 reset(30000);exit_checks=exit_due=exit_frames=0;sleep_exit_left=-5000;queued=100;audio_cost=14000;
 screenWait(1);assert(order[0]=='A'&&copies.size()==1&&exit_frames==1&&!safe_depth&&!action_depth);
 reset(30000);exit_checks=exit_due=exit_frames=0;sleep_calls=0;screenWait(0);screenWait(-1);assert(!sleep_calls&&!exit_checks);
 xu4.screenImage=nullptr;screenWait(1);assert(sleep_calls==1&&!exit_checks&&copies.empty());
 reset(30000);exit_checks=exit_due=exit_frames=0;have_swap=false;screenWait(1);assert(!exit_checks&&copies.empty());
 reset(30000);exit_checks=exit_due=exit_frames=0;in_scheduler=true;screenWait(1);assert(!exit_checks&&copies.empty());
 // Even when audio after the new copy overruns again, this hook does not retry.
 reset(30000);exit_checks=exit_due=exit_frames=0;sleep_exit_left=-1;audio_cost=50000;
 screenWait(1);assert(copies.size()==1&&exit_frames==1&&now>present_clock.deadline);
 assert(!safe_depth&&!action_depth);
 xu4_schedule_report(now);assert(!exit_checks&&!exit_due&&!exit_frames&&!exit_max_us);
 puts("PASS actual screenWait: scope/order, overdue-only, low queue, first-frame/no-image/zero/negative/reentry guards, action-counter integration, no retry after audio overrun");
 puts("PASS actual backend: 6000us boundary, overdue/low-audio/disabled/reentrant guards, one phase-preserving presentation, no duplicate outer wait, key/pad ordering, wrap and report resets");
}
