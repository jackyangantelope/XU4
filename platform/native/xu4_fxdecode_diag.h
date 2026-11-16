// Fixed-size event storage; no formatting, allocation or UART during decoding.
#pragma once
#include <cstdint>
#include <cstdio>
struct Xu4FxDecodeDiag {
    struct Event {uint64_t start_us,us;int id,samples;unsigned truncated;char path[128];};
    Event events[8]={},slowest={};
    unsigned head=0,count=0,total=0,dropped=0;
    void record(int id,const char *path,uint64_t started,uint64_t elapsed,int samples){
        Event e={};e.start_us=started;e.us=elapsed;e.id=id;e.samples=samples;
        unsigned i=0;for(;i<sizeof(e.path)-1&&path[i];++i){unsigned char ch=path[i];e.path[i]=(ch<32||ch==127)?'?':char(ch);}e.path[i]=0;e.truncated=path[i]!=0;
        if(!total||elapsed>slowest.us)slowest=e;++total;
        if(count==8){++dropped;return;}
        events[(head+count)%8]=e;++count;
    }
    void report(uint64_t now){
        // The maximum includes events lost to ring overflow; details drain later.
        printf("XU4_FXDECODE_WIN ms=%lu calls=%u dropped=%u pending=%u max_id=%d max_us=%llu max_start_us=%llu max_samples=%d truncated=%u file=%s\n",(unsigned long)(now/1000),total,dropped,count,total?slowest.id:-1,(unsigned long long)(total?slowest.us:0),(unsigned long long)(total?slowest.start_us:0),total?slowest.samples:0,total?slowest.truncated:0,total?slowest.path:"-");
        total=dropped=0;
        for(unsigned n=0;n<2&&count;++n){const Event &e=events[head];
            printf("XU4_FXDECODE ms=%lu start_us=%llu id=%d decode_us=%llu samples=%d truncated=%u file=%s\n",(unsigned long)(now/1000),(unsigned long long)e.start_us,e.id,(unsigned long long)e.us,e.samples,e.truncated,e.path);
            head=(head+1)%8;--count;
        }
    }
};
