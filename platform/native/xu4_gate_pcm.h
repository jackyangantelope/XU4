// Fixed two-page PCM reader. Header is six little-endian uint32 words.
#pragma once
#include <cstdio>
#include <cstdint>
#include <algorithm>
struct Xu4GatePcm {
    static constexpr unsigned page_samples=4096;
    FILE *file=nullptr;
    int16_t data[2][page_samples];
    uint32_t tag[2]={UINT32_MAX,UINT32_MAX};
    unsigned last_slot=0;
    uint32_t frames=0,rate=0,channels=0;
    unsigned opens=0,errors=0,reads=0,demand=0;
    uint64_t read_us=0,read_max=0,open_max=0;
    uint32_t min_ahead=UINT32_MAX;
    void close(){if(file)fclose(file);file=nullptr;tag[0]=tag[1]=UINT32_MAX;}
    bool page(uint32_t number,bool urgent){
        for(unsigned i=0;i<2;++i)if(tag[i]==number){last_slot=i;return true;}
        unsigned slot=1-last_slot;
        if(!file)return false;
        uint64_t started=time_us_64();
        uint32_t first=number*page_samples,total=frames*channels;
        unsigned count=std::min(uint32_t(page_samples),total-first);
        bool ok=first<total && !fseek(file,24+first*2,SEEK_SET) && fread(data[slot],2,count,file)==count;
        uint64_t us=time_us_64()-started;++reads;read_us+=us;read_max=std::max(read_max,us);if(urgent)++demand;
        if(!ok){++errors;close();return false;}
        tag[slot]=number;last_slot=slot;return true;
    }
    bool open(const char *path){
        close();uint64_t started=time_us_64();
        file=fopen(path,"rb");
        uint32_t h[6]={};
        bool ok=file && fread(h,4,6,file)==6 && h[0]==0x4d503458 && h[1]==1 && h[2]>=8000 && h[2]<=96000 && (h[3]==1||h[3]==2) && h[4]>0 && h[4]<=4000000 && h[5]==h[4]*h[3]*2;
        if(ok){rate=h[2];channels=h[3];frames=h[4];ok=!fseek(file,0,SEEK_END)&&ftell(file)==long(24+h[5]);}
        if(ok)ok=page(0,false);
        if(ok && frames*channels>page_samples)ok=page(1,false);
        open_max=std::max(open_max,time_us_64()-started);
        if(!ok){++errors;close();return false;}++opens;return true;
    }
    // Called once per output block, not once per sample. Prefetch next page
    // after moving into the current page; total memory never grows.
    bool prepare(size_t pos,bool loop){
        if(!file)return false;
        uint32_t number=uint32_t(pos)*channels/page_samples;
        if(!page(number,true))return false;
        uint32_t next=number+1;
        if(next*page_samples>=frames*channels){if(!loop)return true;next=0;}
        return page(next,false);
    }
    int sample(size_t frame,unsigned ch){
        uint32_t index=uint32_t(frame)*channels+(channels==1?0:ch),number=index/page_samples;
        if(!page(number,true))return 0;
        return data[last_slot][index%page_samples];
    }
    void observe(size_t pos){
        if(!file)return;
        uint32_t number=uint32_t(pos)*channels/page_samples;
        uint32_t end=std::min(frames,(number+1)*page_samples/channels);
        if((tag[0]==number+1||tag[1]==number+1))end=std::min(frames,(number+2)*page_samples/channels);
        min_ahead=std::min(min_ahead,uint32_t(end-pos));
    }
};
