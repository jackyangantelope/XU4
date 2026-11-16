#include "xu4_timeline.h"
#include "xu4_read_diag.h"
/* SPDX-License-Identifier: BSD-3-Clause
 * Original IT/OGG assets are decoded, never replaced by generated effects.
 * The original MIDI fanfare uses an offline-decoded PCM sidecar; provenance
 * and synthesizer details must accompany that sidecar in the game package. */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <string>
#include <xmp.h>
#include "native_hal.h"
#include "xu4_profile.h"
#include "sound.h"
#include "config.h"
#include "context.h"
#include "settings.h"
#include "xu4.h"
#ifdef XU4_PICO
#include "pico/time.h"
#include "xu4_audio_budget.h"
#include "xu4_tail_diag.h"
#endif
extern "C" int stb_vorbis_decode_filename(const char*,int*,int*,short**);
static xmp_context module;
static FILE *midi_pcm;
static int track=-1,music_volume=10,fx_volume=10;
static bool enabled,initialized,servicing;
static int16_t *fx;
static size_t fx_samples,fx_pos;
static int fx_channels,fx_rate;
static uint64_t fx_phase,fx_remaining;
static bool fx_loop;
static int gain=65536,fade_from=65536,fade_to=65536;
static uint32_t fade_position,fade_length;
static bool playing;
static bool effect_active();
static int16_t buffer[882];
#ifdef XU4_PICO
struct CachedClip {
    Sound id;
    int16_t *pcm;
    int samples, channels, rate;
};
#include "xu4_gate_pcm.h"
#include "xu4_fxdecode_diag.h"
#include "xu4_track_diag.h"
static Xu4TrackDiag track_diag;
static Xu4FxDecodeDiag fx_decode_diag;
static Xu4GatePcm gate_pcm;
static bool gate_active;
// One reader and one 16KiB buffer, shared by the two single-voice effects.
static int pcm_asset=-1; // 0 title, 1 gate; retained after close for statistics.
struct PcmStats {
    unsigned opens=0,reads=0,demand=0,errors=0;
    uint64_t read_us=0,read_max=0,open_max=0;
    uint32_t min_ahead=UINT32_MAX;
};
static PcmStats pcm_stats[2];
static void pcm_collect_stats(){
    if(pcm_asset>=0){auto &s=pcm_stats[pcm_asset];
        s.opens+=gate_pcm.opens;s.reads+=gate_pcm.reads;s.demand+=gate_pcm.demand;s.errors+=gate_pcm.errors;
        s.read_us+=gate_pcm.read_us;s.read_max=std::max(s.read_max,gate_pcm.read_max);s.open_max=std::max(s.open_max,gate_pcm.open_max);s.min_ahead=std::min(s.min_ahead,gate_pcm.min_ahead);
    }
    gate_pcm.opens=gate_pcm.reads=gate_pcm.demand=gate_pcm.errors=0;
    gate_pcm.read_us=gate_pcm.read_max=gate_pcm.open_max=0;gate_pcm.min_ahead=UINT32_MAX;
}
static void pcm_report(uint64_t now){
    pcm_collect_stats();
    for(int i=0;i<2;++i){auto &s=pcm_stats[i];
        printf("XU4_%s_PCM ms=%lu id=%d file=%s opens=%u active=%u buffer_bytes=16384 shared=1 reads=%u read_us=%llu read_max_us=%llu open_max_us=%llu demand_reads=%u errors=%u min_ahead_frames=%u\n",i?"GATE":"TITLE",(unsigned long)(now/1000),int(i?SOUND_GATE_OPEN:SOUND_TITLE_FADE),i?"gate_open.ogg":"title_fade.ogg",s.opens,unsigned(gate_active&&pcm_asset==i),s.reads,(unsigned long long)s.read_us,(unsigned long long)s.read_max,(unsigned long long)s.open_max,s.demand,s.errors,s.min_ahead==UINT32_MAX?0:s.min_ahead);
        s=PcmStats{};
    }
}
static CachedClip cached_clips[] = {{SOUND_WALK_NORMAL}, {SOUND_WALK_SLOWED}, {SOUND_WALK_COMBAT}, {SOUND_BLOCKED}};
static constexpr size_t cache_budget = 512 * 1024;
static size_t cache_bytes;
static bool fx_cached;
static unsigned footstep_hits, footstep_decodes;
static uint64_t footstep_decode_max_us;
static unsigned blocked_hits, blocked_decodes;
static uint64_t blocked_decode_max_us;
extern "C" void xu4_reset_footstep_stats() {
    footstep_hits = footstep_decodes = blocked_hits = blocked_decodes = 0;
    footstep_decode_max_us = blocked_decode_max_us = 0;
}

extern "C" void xu4_report_footstep_cache(uint64_t presented_at) {
    track_diag.report(presented_at);
    xu4_read_report(presented_at);
    fx_decode_diag.report(presented_at);
    printf("XU4_FXCACHE ms=%lu mode=ON hits=%u decodes=%u decode_max_us=%llu pcm_bytes=%lu blocked_hits=%u blocked_decodes=%u blocked_decode_max_us=%llu\n",
           (unsigned long)(presented_at / 1000),
           footstep_hits, footstep_decodes, (unsigned long long)footstep_decode_max_us,
           (unsigned long)cache_bytes, blocked_hits, blocked_decodes,
           (unsigned long long)blocked_decode_max_us);
    pcm_report(presented_at);
    footstep_hits = footstep_decodes = 0;
    footstep_decode_max_us = 0;
    blocked_hits = blocked_decodes = 0;
    blocked_decode_max_us = 0;
}
#endif
static bool effect_active(){
#ifdef XU4_PICO
    return fx || gate_active;
#else
    return fx;
#endif
}
static void stop_track(){
    if(module){xmp_end_player(module);xmp_release_module(module);xmp_free_context(module);module=NULL;}
    if(midi_pcm){fclose(midi_pcm);midi_pcm=NULL;}
    playing=false;track=-1;
}
#ifdef XU4_PICO
#define TRACK_TIME(field,expr) trace.measure(&Xu4TrackDiag::Event::field,[&](){return (expr);})
#define TRACK_STATE(code,value) do{trace.e.result=(code);trace.e.rc=(value);}while(0)
#else
#define TRACK_TIME(field,expr) (expr)
#define TRACK_STATE(code,value) ((void)0)
#endif
static bool load_track(int id){
    XU4_PROFILE_SCOPE(XPROF_TRACK);
    if(id<=MUSIC_NONE||id>=MUSIC_MAX)return false;
    if(track==id&&playing)return true;
    const char *path=xu4.config->musicFile(id);if(!path)return false;
#ifdef XU4_PICO
    Xu4TrackTrace trace(track_diag,id,track,path);
#endif
    std::string filename(path);TRACK_TIME(stop_us,(stop_track(),0));
#ifdef XU4_PICO
    trace.e.used_stopped=mallinfo().uordblks;
#endif
    if(filename.size()>=4&&filename.substr(filename.size()-4)==".mid"){
        filename+=".pcm";
#ifdef XU4_PICO
        trace.set_path(filename.c_str());
#endif
        midi_pcm=TRACK_TIME(load_us,fopen(filename.c_str(),"rb"));
#ifdef XU4_PICO
        trace.e.used_loaded=mallinfo().uordblks;
#endif
        if(!midi_pcm){TRACK_STATE(4,-1);fprintf(stderr,"Missing original-MIDI PCM sidecar: %s\n",filename.c_str());return false;}
    }else{
        module=TRACK_TIME(create_us,xmp_create_context());
        if(!module){TRACK_STATE(1,-1);return false;}
#ifdef XU4_PICO
        xu4_read_begin(id,filename.c_str());
#endif
        int rc=TRACK_TIME(load_us,xmp_load_module(module,filename.c_str()));
#ifdef XU4_PICO
        xu4_read_end(rc);
#endif
#ifdef XU4_PICO
        trace.e.used_loaded=mallinfo().uordblks;
#endif
        if(rc!=0){TRACK_STATE(2,rc);xmp_free_context(module);module=NULL;return false;}
        rc=TRACK_TIME(start_player_us,xmp_start_player(module,44100,0));
        if(rc!=0){TRACK_STATE(3,rc);xmp_release_module(module);xmp_free_context(module);module=NULL;return false;}
        TRACK_TIME(config_us,xmp_set_player(module,XMP_PLAYER_INTERP,XMP_INTERP_LINEAR));
    }
    track=id;playing=true;gain=65536;fade_length=0;
    TRACK_STATE(0,0);
    return true;
}
#undef TRACK_TIME
#undef TRACK_STATE
int soundInit(){initialized=true;enabled=xu4.settings->musicVol>0;
    music_volume=xu4.settings->musicVol;fx_volume=xu4.settings->soundVol;
#ifdef XU4_PICO
    puts("XU4_FXCACHE_BOOT mode=ON forced=1 blocked_cache=1");
#endif
    return 1;
}
void soundStop(){
#ifdef XU4_PICO
    gate_pcm.close();gate_active=false;
#endif
#ifdef XU4_PICO
    if (!fx_cached)
#endif
        free(fx);
#ifdef XU4_PICO
    fx_cached = false;
#endif
    fx=NULL;fx_samples=fx_pos=0;fx_remaining=0;
}
void soundDelete(){
    initialized=false;stop_track();soundStop();
#ifdef XU4_PICO
    for (auto &clip : cached_clips) {
        free(clip.pcm);
        clip.pcm = NULL;
    }
    cache_bytes = 0;
    footstep_hits = footstep_decodes = 0;
    footstep_decode_max_us = 0;
    blocked_hits = blocked_decodes = 0;
    blocked_decode_max_us = 0;
#endif
}
void soundPlay(Sound id,bool only_once,int duration){
    if(!initialized||!fx_volume||id<0||id>=SOUND_MAX||(only_once&&fx_remaining))return;
    int n;
#ifdef XU4_PICO
    if(id==SOUND_GATE_OPEN||id==SOUND_TITLE_FADE){
        const char *path=xu4.config->soundFile(id);if(!path)return;
        std::string filename(path);
        // Only these two audited original assets use PCM sidecars.
        auto slash=filename.find_last_of("/\\");
        if(filename.substr(slash==std::string::npos?0:slash+1)==(id==SOUND_GATE_OPEN?"gate_open.ogg":"title_fade.ogg")){
            filename+=".pcm";soundStop();pcm_collect_stats();pcm_asset=id==SOUND_TITLE_FADE?0:1;
            if(!gate_pcm.open(filename.c_str())){fprintf(stderr,"XU4_PCM_ERROR id=%d open=%s\n",int(id),filename.c_str());return;}
            gate_active=true;fx_channels=gate_pcm.channels;fx_rate=gate_pcm.rate;
            fx_samples=gate_pcm.frames;fx_phase=0;fx_pos=0;fx_loop=duration>=0;
            fx_remaining=fx_loop?uint64_t(duration)*44100/1000:(uint64_t(fx_samples)*44100+fx_rate-1)/fx_rate;
            return;
        }
    }
#endif
#ifdef XU4_PICO
    CachedClip *cached_clip = NULL;
    for (auto &clip : cached_clips) {
        if (clip.id == id) {
            cached_clip = &clip;
            break;
        }
    }
    if (cached_clip && cached_clip->pcm) {
        soundStop();
        fx = cached_clip->pcm;
        fx_channels = cached_clip->channels;
        fx_rate = cached_clip->rate;
        n = cached_clip->samples;
        fx_cached = true;
        if (id == SOUND_BLOCKED) ++blocked_hits;
        else ++footstep_hits;
    } else
#endif
    {
        const char *p=xu4.config->soundFile(id);if(!p)return;
        std::string filename(p);soundStop();
#ifdef XU4_PICO
        const uint64_t decode_start = time_us_64();
#endif
        {
            XU4_PROFILE_SCOPE(XPROF_DECODE);
            n=stb_vorbis_decode_filename(filename.c_str(),&fx_channels,&fx_rate,&fx);
        }
#ifdef XU4_PICO
        const uint64_t elapsed = time_us_64() - decode_start;
        fx_decode_diag.record(int(id),filename.c_str(),decode_start,elapsed,n);
        if (cached_clip) {
            if (id == SOUND_BLOCKED) {
                ++blocked_decodes;
                blocked_decode_max_us = std::max(blocked_decode_max_us, elapsed);
            } else {
                ++footstep_decodes;
                footstep_decode_max_us = std::max(footstep_decode_max_us, elapsed);
            }
        }
#endif
        if(n<=0||fx_channels<1||fx_channels>2||fx_rate<=0){soundStop();fprintf(stderr,"OGG decode failed: %s\n",filename.c_str());return;}
#ifdef XU4_PICO
        const size_t frame_bytes = fx_channels * sizeof(*fx);
        if (cached_clip && size_t(n) <= (cache_budget - cache_bytes) / frame_bytes) {
            // The decoder grows its allocation geometrically. Retain only the PCM size.
            const size_t bytes = size_t(n) * frame_bytes;
            if (auto *pcm = static_cast<int16_t *>(realloc(fx, bytes))) {
                fx = pcm;
                *cached_clip = {id, pcm, n, fx_channels, fx_rate};
                cache_bytes += bytes;
                fx_cached = true;
            }
        }
#endif
    }
    fx_samples=n;fx_phase=0;fx_pos=0;fx_loop=duration>=0;
    fx_remaining=fx_loop?uint64_t(duration)*44100/1000:(uint64_t(n)*44100+fx_rate-1)/fx_rate;
}
void musicPlay(int id){if(initialized&&enabled)load_track(id);}
void musicPlayLocale(){if(c&&c->location)musicPlay(c->location->map->music);}
void musicStop(){stop_track();}
static void fade(int to,int ms){
    fade_from=gain;fade_to=to;fade_position=0;fade_length=uint64_t(std::max(ms,1))*44100/1000;
}
void musicFadeOut(int ms){if(playing){if(xu4.settings->volumeFades)fade(0,ms);else stop_track();}}
void musicFadeIn(int ms,bool from_map){
    if(!enabled||!initialized)return;
    if(from_map||!playing)musicPlayLocale();
    if(playing&&xu4.settings->volumeFades){gain=0;fade(65536,ms);}
}
void musicSetVolume(int v){music_volume=std::max(0,std::min(v,10));}
void soundSetVolume(int v){fx_volume=std::max(0,std::min(v,10));}
int musicVolumeDec(){if(xu4.settings->musicVol>0)musicSetVolume(--xu4.settings->musicVol);return xu4.settings->musicVol*10;}
int musicVolumeInc(){if(xu4.settings->musicVol<10)musicSetVolume(++xu4.settings->musicVol);return xu4.settings->musicVol*10;}
int soundVolumeDec(){if(xu4.settings->soundVol>0)soundSetVolume(--xu4.settings->soundVol);return xu4.settings->soundVol*10;}
int soundVolumeInc(){if(xu4.settings->soundVol<10)soundSetVolume(++xu4.settings->soundVol);return xu4.settings->soundVol*10;}
bool musicToggle(){enabled=!enabled;if(enabled)musicFadeIn(1000,true);else musicFadeOut(1000);return enabled;}
extern "C" void xu4_native_audio_service(){
    if(!initialized||servicing)return;servicing=true;
    XU4_PROFILE_SCOPE(XPROF_AUDIO);

#ifdef XU4_PICO
    const uint64_t call_started = time_us_64();
    uint64_t parts[4] = {};
    Xu4BlockTrace diag_blocks[10];
    unsigned processed_blocks = 0, video_presents = 0, video_defers = 0;
    const unsigned queue_start = native_audio_queued();
    uint64_t block_max_us = 0, video_us = 0;
#endif
    /* 4096-frame HW queue: leave space for the entire 441-frame write. */
    for(int blocks=0;blocks<10&&native_audio_queued()<3528;blocks++){

#ifdef XU4_PICO
        const unsigned queued = native_audio_queued();
        const int64_t wait_left=xu4_diag_wait_left(time_us_64());
        if(xu4_wait_audio_can_yield(queued,wait_left)){xu4_diag_wait_yield(1);break;}
        if(wait_left<=0 && queued<1764)xu4_diag_wait_yield(2);
        const unsigned reason = xu4_audio_yield_reason(queued, processed_blocks,
            time_us_64() - call_started - video_us, xu4_video_remaining_ms());
        if (reason) { xu4_prof_audio_yield(reason); break; }
        if (queued < 1764) xu4_prof_audio_yield(3);
        ++processed_blocks;
        uint64_t part_started = time_us_64();
        const uint64_t block_started = part_started;
        auto &diag_block=diag_blocks[processed_blocks-1];
        diag_block.before=queued;
        diag_block.video_before=xu4_video_left_us(block_started);
        diag_block.wait_before=xu4_diag_wait_left(block_started);
#endif
        memset(buffer,0,sizeof buffer);
        if(playing&&module&&xmp_play_buffer(module,buffer,sizeof buffer,0)<0)stop_track();

#ifdef XU4_PICO
        uint64_t part_now = time_us_64();
        parts[0] += part_now - part_started; part_started = part_now;
#endif
        if(playing&&midi_pcm){
            size_t n=fread(buffer,sizeof(int16_t)*2,441,midi_pcm);
            if(n<441){if(ferror(midi_pcm)){stop_track();}else{
                rewind(midi_pcm);size_t more=fread(buffer+n*2,4,441-n,midi_pcm);
                if(more==0)stop_track();
            }}
        }

#ifdef XU4_PICO
        if(gate_active){
            gate_pcm.observe(fx_pos);
            if(!gate_pcm.prepare(fx_pos,fx_loop))soundStop();
        }
        part_now = time_us_64();
        parts[1] += part_now - part_started; part_started = part_now;
#endif
        for(unsigned i=0;i<441;i++){
            if(fade_length){
                gain=fade_from+int(int64_t(fade_to-fade_from)*++fade_position/fade_length);
                if(fade_position>=fade_length){gain=fade_to;fade_length=0;if(!gain)stop_track();}
            }
            for(unsigned ch=0;ch<2;ch++){
                int mixed=int(int64_t(buffer[i*2+ch])*music_volume*gain/(10*65536));
                if(effect_active()&&fx_remaining){
                    size_t next=fx_pos+1;
                    if(next>=fx_samples)next=fx_loop?0:fx_pos;
                    int a,b;
#ifdef XU4_PICO
                    if(gate_active){a=gate_pcm.sample(fx_pos,ch);b=gate_pcm.sample(next,ch);if(!gate_pcm.file){soundStop();a=b=0;}}
                    else
#endif
                    {a=fx[fx_pos*fx_channels+(fx_channels==1?0:ch)];
                     b=fx[next*fx_channels+(fx_channels==1?0:ch)];}
                    mixed+=(a+int(int64_t(b-a)*fx_phase/44100))*fx_volume/10;
                }
                buffer[i*2+ch]=int16_t(std::max(-32768,std::min(mixed,32767)));
            }
            if(effect_active()&&fx_remaining){
                fx_phase+=fx_rate;fx_pos+=fx_phase/44100;fx_phase%=44100;--fx_remaining;
                if(fx_pos>=fx_samples){if(fx_loop)fx_pos%=fx_samples;else fx_remaining=0;}
                if(!fx_remaining)soundStop();
            }
        }

#ifdef XU4_PICO
        part_now = time_us_64();
        parts[2] += part_now - part_started; part_started = part_now;
#endif
        native_audio_submit(buffer,441);
#ifdef XU4_PICO
        const uint64_t block_finished = time_us_64();
        parts[3] += block_finished - part_started;
        block_max_us = std::max(block_max_us, block_finished - block_started);
        // A completed PCM block is safe to pause. Keep synthesis state intact.
        // Do not charge framebuffer work to the audio category or soft audio budget.
        const unsigned video_left = xu4_video_remaining_ms();
        const unsigned queued_after = native_audio_queued();
        diag_block.after=queued_after;diag_block.us=block_finished-block_started;
        diag_block.video_after=xu4_video_left_us(block_finished);
        diag_block.wait_after=xu4_diag_wait_left(block_finished);
        xu4_timeline_block(block_started,block_finished,queued,queued_after,diag_block.video_before,diag_block.wait_before);
        if (video_left == 0 && queued_after < 882) ++video_defers;
        if (xu4_audio_video_ready(queued_after, video_left)) {
            // Return to the foreground scheduler; never present from audio.
            break;
        }
#endif

    }

#ifdef XU4_PICO
    xu4_diag_audio(time_us_64(),time_us_64()-call_started,processed_blocks,diag_blocks);
    xu4_prof_audio_call(time_us_64() - call_started - video_us, processed_blocks, parts);
    xu4_prof_audio_recovery(queue_start, processed_blocks, block_max_us, video_presents, video_us, video_defers);
#endif
    servicing=false;
}
