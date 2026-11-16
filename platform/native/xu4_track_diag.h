// Core0-only bounded completed-load diagnostics. No allocation or UART in record.
#pragma once
#include <cstdint>
#include <cstdio>
#include <malloc.h>
struct Xu4TrackDiag {
 struct Event {
  uint64_t start_us=0,total_us=0,stop_us=0,create_us=0,load_us=0,start_player_us=0,config_us=0;
  uint32_t used_before=0,used_stopped=0,used_loaded=0,used_after=0,arena_before=0,arena_after=0;
  int id=-1,old_id=-1,result=-1,rc=0;unsigned truncated=0;char file[128]={};
 } events[8],slowest;
 unsigned head=0,count=0,calls=0,dropped=0;
 void record(const Event &e){if(!calls||e.total_us>slowest.total_us)slowest=e;++calls;if(count==8){++dropped;return;}events[(head+count)%8]=e;++count;}
 static void emit(const char *kind,uint64_t now,const Event &e){
  printf("XU4_TRACK_%s ms=%lu start_us=%llu id=%d old_id=%d result=%d rc=%d total_us=%llu stop_us=%llu create_us=%llu load_us=%llu start_player_us=%llu config_us=%llu heap_before=%lu heap_stopped=%lu heap_loaded=%lu heap_after=%lu arena_before=%lu arena_after=%lu truncated=%u file=%s\n",kind,(unsigned long)(now/1000),(unsigned long long)e.start_us,e.id,e.old_id,e.result,e.rc,(unsigned long long)e.total_us,(unsigned long long)e.stop_us,(unsigned long long)e.create_us,(unsigned long long)e.load_us,(unsigned long long)e.start_player_us,(unsigned long long)e.config_us,(unsigned long)e.used_before,(unsigned long)e.used_stopped,(unsigned long)e.used_loaded,(unsigned long)e.used_after,(unsigned long)e.arena_before,(unsigned long)e.arena_after,e.truncated,e.file);
 }
 void report(uint64_t now){
  printf("XU4_TRACK_WIN ms=%lu calls=%u pending=%u dropped=%u\n",(unsigned long)(now/1000),calls,count,dropped);
  if(calls)emit("MAX",now,slowest);
  if(count){const Event &e=events[head];if(!calls||e.start_us!=slowest.start_us||e.id!=slowest.id)emit("LOAD",now,e);head=(head+1)%8;--count;}
  calls=dropped=0;
 }
};
struct Xu4TrackTrace {
 Xu4TrackDiag &sink;Xu4TrackDiag::Event e;
 Xu4TrackTrace(Xu4TrackDiag &target,int id,int old,const char *path):sink(target){
  e.start_us=time_us_64();e.id=id;e.old_id=old;
  auto h=mallinfo();e.used_before=h.uordblks;e.arena_before=h.arena;
  set_path(path);
 }
 void set_path(const char *path){unsigned i=0;for(;i<sizeof(e.file)-1&&path[i];++i){unsigned char c=path[i];e.file[i]=c<32||c==127?'?':char(c);}e.file[i]=0;e.truncated=path[i]!=0;}
 template<class F> auto measure(uint64_t Xu4TrackDiag::Event::*field,F fn)->decltype(fn()){
  auto at=time_us_64();auto result=fn();e.*field=time_us_64()-at;return result;
 }
 ~Xu4TrackTrace(){auto h=mallinfo();e.used_after=h.uordblks;e.arena_after=h.arena;e.total_us=time_us_64()-e.start_us;sink.record(e);}
};
