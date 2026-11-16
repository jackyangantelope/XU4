/* SPDX-License-Identifier: BSD-3-Clause
 * Native xu4 display/input backend. No SDL in the game core. */
#include <cstring>
#include <vector>
#include "native_hal.h"
#include "xu4_profile.h"
#include "xu4_wait.h"
#include "xu4_audio_budget.h"
#include "xu4_present_clock.h"
#include "xu4_tail_diag.h"
#ifdef XU4_PICO
#include "pico/stdlib.h"
#endif
#include <algorithm>
#include "xu4.h"
#include "settings.h"
#include "screen.h"
#include "image.h"
#include "event.h"
#include "pad_native.h"
static Xu4PadState pad;
static int frame_ms=40;
static std::vector<uint32_t> overlay;
static void present_complete_image();
#ifdef XU4_PICO
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
#endif

void screenInit_sys(const Settings *s,int *dim,int) {
    /* Both transports expose the actual 320x200 image. Reinitializing video
     * from the game's options must not allocate a 2x/3x framebuffer. */
    xu4.settings->scale=1;
    dim[0]=dim[2]=320;dim[1]=dim[3]=200;
    screenState()->formatIsABGR=true;
    frame_ms=1000/(s->screenAnimationFramesPerSecond?s->screenAnimationFramesPerSecond:25);
#ifdef XU4_PICO
    present_clock.period=1000000/(s->screenAnimationFramesPerSecond?s->screenAnimationFramesPerSecond:25);
    present_clock.started=false;have_swap=false;
#endif
}
void screenDelete_sys(){overlay.clear();}
void screenIconify(){}
void screenSetMouseCursor(MouseCursor){}
void screenShowMouseCursor(bool){}
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
static void present_complete_image(){
    Image *im=xu4.screenImage;if(!im)return;
    if(pad.keyboard){
        const size_t count=size_t(im->w)*im->h;
        overlay.assign(im->pixels,im->pixels+count);
        im->fillRect(0,120,320,80,0,0,0);
        screenTextAt(0,15,"SELECT CLOSE  A TYPE  Y CASE  START ENTER");
        for(unsigned i=0;i<40;i++){
            char label[5];
            if(i<36){char c="abcdefghijklmnopqrstuvwxyz0123456789"[i];
                if(pad.uppercase&&c>='a'&&c<='z')c-=32;
                snprintf(label,sizeof label," %c ",c);
            }else{const char *special[]={"SPC","ENT","DEL","ESC"};strcpy(label,special[i-36]);}
            screenTextAt((i%8)*5,17+i/8,"%c%s",pad.selected==i?'>':' ',label);
        }
    }
    native_present(im->pixels,im->w,im->h,screenState()->vertOffset);
    if(!overlay.empty()){
        memcpy(im->pixels,overlay.data(),overlay.size()*sizeof(uint32_t));overlay.clear();
    }
}

static int hid_key(uint8_t k,uint8_t mod){
    const bool shift=mod&0x22;
    int key=0;
    if(k>=4&&k<=29)key=(shift?'A':'a')+k-4;
    else if(k>=30&&k<=39)key=(shift?"!@#$%^&*()":"1234567890")[k-30];
    else if(k>=58&&k<=69)key=U4_FKEY+k-58;
    else switch(k){
      case 40:key=U4_ENTER;break;case 41:key=U4_ESC;break;
      case 42:case 76:key=U4_BACKSPACE;break;case 43:key='\t';break;case 44:key=' ';break;
      case 45:key=shift?'_':'-';break;case 46:key=shift?'+':'=';break;
      case 47:key=shift?'{':'[';break;case 48:key=shift?'}':']';break;
      case 49:key=shift?'|':'\\';break;case 51:key=shift?':':';';break;
      case 52:key=shift?'"':'\'';break;case 53:key=shift?'~':'`';break;
      case 54:key=shift?'<':',';break;case 55:key=shift?'>':'.';break;
      case 56:key=shift?'?':'/';break;
      case 79:key=U4_RIGHT;break;case 80:key=U4_LEFT;break;case 81:key=U4_DOWN;break;case 82:key=U4_UP;break;
    }
    if((mod&0x11)&&k>=4&&k<=29)key=k-3;
    if(key&&(mod&0x44))key+=U4_ALT;
    if(key&&(mod&0x88))key+=U4_META;
    return key;
}
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
int EventHandler::setKeyRepeat(int delay,int interval){native_key_repeat(delay>0?delay:0,interval>0?interval:0);return 0;}
