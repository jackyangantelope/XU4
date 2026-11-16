
#include "xu4_timeline.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <malloc.h>
#include <unistd.h>
#include "pico/stdlib.h"
#include "hardware/psram.h"
#include "jack_wave1_runtime.h"
#include "jack_wave1_audio.h"
#include "native_hal.h"
#include "xu4_profile.h"
#include "xu4_boot_diag.h"
#include "xu4_integrity.h"
#include "xu4_tail_diag.h"
#include "native_game_menu.h"
extern "C" unsigned native_hw_audio_level(void);
extern "C" bool native_usb_key_held(uint8_t key);
extern "C" void xu4_report_footstep_cache(uint64_t presented_at);
extern "C" void xu4_schedule_service();
extern "C" void xu4_schedule_report(uint64_t now);
static uint64_t fresh_frames, repeat_frames, changed_pixels;
static bool seen_frame;
static bool compare_enabled=false, compare_requested=false;
static uint64_t measured_frames;
static void request_compare_toggle(){
    compare_requested=!compare_requested;
    printf("XU4_COMPARE requested=%s apply=next_report_boundary\n",compare_requested?"ON":"OFF");
}
static uint8_t repeat_key;
static uint32_t repeat_at;
static unsigned repeat_delay=350,repeat_interval=120;
extern int xu4_game_main(int,char**);
extern "C" uint32_t native_millis(){return to_ms_since_boot(get_absolute_time());}
extern "C" void xu4_wait_until_us(uint64_t deadline){
    XU4_PROFILE_SCOPE(XPROF_SLEEP);
    Xu4TailWait diag_wait(deadline,xu4_in_action_wait());
    for(;;){
        uint64_t diag_start=time_us_64();jack_wave1_usb_service();
        xu4_diag_stage(TAIL_USB,diag_start,time_us_64());
        xu4_schedule_service();
        diag_start=time_us_64();xu4_uart_service();
        xu4_diag_stage(TAIL_UART,diag_start,time_us_64());
        const uint64_t now=time_us_64();
        if(now>=deadline)break;
        const uint64_t left=deadline-now;
        const uint64_t diag_sleep=time_us_64();
        sleep_us(left<500?unsigned(left):500);
        xu4_diag_stage(TAIL_SLEEP,diag_sleep,time_us_64());
    }
}
extern "C" void native_sleep(unsigned ms){
    xu4_wait_until_us(time_us_64()+uint64_t(ms)*1000);
}
extern "C" int native_key(uint8_t *usage,uint8_t *mod){
    xu4_uart_service();
    jack_wave1_usb_service();jack_wave1_key_event_t key;
    if(jack_wave1_usb_pop_key(&key)){

        if(key.keycode==65 && (key.modifier&0x11) && (key.modifier&0x44)){
            repeat_key=0;request_compare_toggle();return 0;
        }
        *usage=repeat_key=key.keycode;*mod=key.modifier;repeat_at=native_millis()+repeat_delay;return 1;
    }
    if(repeat_key&&!native_usb_key_held(repeat_key))repeat_key=0;
    if(repeat_key&&repeat_delay&&repeat_interval&&int32_t(native_millis()-repeat_at)>=0){
        *usage=repeat_key;*mod=jack_wave1_usb_current_modifiers();repeat_at=native_millis()+repeat_interval;return 1;
    }
    return 0;
}
extern "C" void native_key_repeat(unsigned delay,unsigned interval){repeat_delay=delay;repeat_interval=interval;repeat_key=0;}
extern "C" uint16_t native_pad(){return jack_wave1_poll_physical_buttons();}
extern "C" void native_keys(uint8_t state[256]){
    jack_wave1_usb_service();
    for(unsigned i=0;i<256;i++)state[i]=native_usb_key_held(uint8_t(i));
    uint8_t mod=jack_wave1_usb_current_modifiers();
    for(unsigned i=0;i<8;i++)state[224+i]=(mod>>i)&1;
}
extern "C" int native_quit(){return 0;}
extern "C" size_t native_audio_queued(){return native_hw_audio_level();}
extern "C" void native_audio_submit(const int16_t *samples,size_t frames){jack_wave1_audio_submit(samples,frames);}
template<bool Compare> static unsigned copy_frame(const uint32_t *pixels,uint16_t *out,int offset){
    unsigned changed=0;
    for(unsigned y=0;y<200;y++){
        const unsigned char *src=(const unsigned char*)pixels+(y>=unsigned(offset)?(y-offset)*320*4:0);
        for(unsigned x=0;x<320;x++){
            const uint16_t value=y<unsigned(offset)?0:uint16_t((src[x*4]>>3)<<10|(src[x*4+1]>>3)<<5|(src[x*4+2]>>3));
            if(Compare)changed += out[(y+20)*320+x]!=value;
            out[(y+20)*320+x]=value;
        }
    }
    return changed;
}
extern "C" void native_present(const uint32_t *pixels,unsigned w,unsigned h,int offset){
    if(w!=320||h!=200)return;
    xu4_prof_enter(XPROF_COPY);
    uint16_t *out=jack_wave1_framebuffer();
    if(offset<0)offset=0;if(offset>199)offset=199;
    const unsigned changed=compare_enabled ? copy_frame<true>(pixels,out,offset) : copy_frame<false>(pixels,out,offset);
    xu4_prof_leave(XPROF_COPY);
    if(seen_frame && compare_enabled){++measured_frames;if(changed)++fresh_frames;else ++repeat_frames;changed_pixels+=changed;}
    seen_frame=true;

    const uint64_t presented_at = time_us_64();
    xu4_prof_frame(presented_at);
    static uint64_t previous_at, window_start;
    static uint64_t min_us, max_us;
    static unsigned intervals;
    if (!previous_at) {
        window_start = presented_at;
        xu4_integrity_finish();
        xu4_boot_finish();
        puts("XU4_BOOT first_frame");
        xu4_uart_enable_queue();
    } else {
        const uint64_t gap = presented_at - previous_at;
        if (!intervals || gap < min_us) min_us = gap;
        if (!intervals || gap > max_us) max_us = gap;
        ++intervals;
        const uint64_t elapsed = presented_at - window_start;
        if (elapsed >= 5000000) {
            const unsigned fps_milli = (uint64_t(intervals) * 1000000000 + elapsed / 2) / elapsed;
            printf("XU4_FPS ms=%lu frames=%u elapsed_us=%llu fps=%u.%03u frame_us_min=%llu frame_us_avg=%llu frame_us_max=%llu\n",
                   (unsigned long)(presented_at / 1000), intervals, (unsigned long long)elapsed,
                   fps_milli / 1000, fps_milli % 1000, (unsigned long long)min_us,
                   (unsigned long long)(elapsed / intervals), (unsigned long long)max_us);
            printf("XU4_PRESENT ms=%lu fresh=%llu repeat=%llu changed_pixels=%llu compare=%s measured=%llu\n", (unsigned long)(presented_at/1000), (unsigned long long)fresh_frames,(unsigned long long)repeat_frames,(unsigned long long)changed_pixels,compare_enabled?"ON":"OFF",(unsigned long long)measured_frames);
            fresh_frames=repeat_frames=changed_pixels=measured_frames=0;
            if(compare_enabled!=compare_requested){
                compare_enabled=compare_requested;
                printf("XU4_COMPARE active=%s start_ms=%lu\n",compare_enabled?"ON":"OFF",(unsigned long)(presented_at/1000));
            }
            xu4_schedule_report(presented_at);
            xu4_report_footstep_cache(presented_at);
            xu4_prof_report(presented_at);
            xu4_diag_report(presented_at);
            xu4_timeline_window(presented_at);
            window_start = presented_at;
            intervals = 0;
        }
    }
    xu4_timeline_drain();
    previous_at = presented_at;
    xu4_uart_service();
    static uint32_t report_at;uint32_t now=native_millis();
    if(now-report_at>30000){report_at=now;struct mallinfo heap=mallinfo();
        printf("XU4_NATIVE ms=%lu heap_used=%lu heap_reserved=%lu\n",(unsigned long)now,(unsigned long)heap.uordblks,(unsigned long)heap.arena);
        jack_wave1_audio_report();
    }
}
int main(){
    if(!jack_wave1_init("Ultima IV native development"))return 1;
    xu4_prof_uart_init();
    puts("XU4_COMPARE_BOOT default=OFF toggle=Ctrl+Alt+F8 apply=next_report_boundary");
    puts("XU4_INPUT keyboard_budget=1 uart_driver_timing=1 quiet=1");
    puts("XU4_WAIT_BOOT action_present=1 loop_remaining_budget=1");
    puts("XU4_LOG_BOOT build=waitexit1 mode=continuous tx=queued_after_first_frame queue_bytes=8192 duration_limit=none report_ms=5000");
    if(!psram_is_available()||psram_get_size()<8u*1024u*1024u){jack_wave1_show_error("PSRAM","8 MiB required");return 2;}
    if(!jack_wave1_mount_sd()){jack_wave1_show_error("SD","Cannot mount SD card");return 3;}
    char selected[JACK_WAVE1_PATH_MAX];
    if(!native_game_select(NATIVE_GAME_XU4,"/roms/XU4",selected,sizeof selected))return 5;
    if(chdir(selected)){jack_wave1_show_error("GAME","Cannot open selected directory");return 6;}
    if(!jack_wave1_audio_begin(44100))return 4;
    char name[]="xu4",scale[]="-s",one[]="1";
    char *args[]={name,scale,one};
    xu4_boot_start();
    xu4_integrity_start(selected);
    xu4_boot_stage(1,"game_main");
    int result=xu4_game_main(3,args);
    printf("XU4_EXIT result=%d\n",result);
    while(true){jack_wave1_usb_service();xu4_uart_service();sleep_ms(1);}
}
