#pragma once
#include <stdint.h>
int64_t xu4_video_left_us(uint64_t now);
bool xu4_in_action_wait();
struct Xu4BlockTrace { unsigned before,after; uint64_t us; int64_t video_before,video_after,wait_before,wait_after; };
void xu4_diag_audio(uint64_t at,uint64_t us,unsigned count,const Xu4BlockTrace *blocks);
enum Xu4TailStage {TAIL_USB,TAIL_AUDIO,TAIL_VIDEO,TAIL_UART,TAIL_SLEEP,TAIL_COUNT};
struct Xu4TailWait {
    Xu4TailWait *parent; uint64_t deadline,start,over[TAIL_COUNT]={},late_starts=0; bool action;
    Xu4TailWait(uint64_t deadline,bool action);
    ~Xu4TailWait();
};
int64_t xu4_diag_wait_left(uint64_t now);
void xu4_diag_stage(unsigned stage,uint64_t start,uint64_t end);
void xu4_diag_report(uint64_t now);

void xu4_diag_wait_yield(unsigned kind); // 0=scheduler skip,1=block boundary,2=low queue continues
