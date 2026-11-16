#pragma once
#include <stdint.h>
#ifdef XU4_PICO
#include "pico/time.h"
struct Xu4DrawState { uintptr_t map; int x,y,z,view,cycle; unsigned flags; uint32_t tiles; bool software; };
unsigned xu4_draw_origin_get();
void xu4_draw_origin_set(unsigned);
void xu4_draw_complete(const Xu4DrawState&, unsigned, const uint64_t*);
void xu4_draw_frame(bool,bool valid=true);
void xu4_draw_report(uint64_t,uint64_t);
class Xu4DrawOrigin {
 unsigned previous;
public:
 Xu4DrawOrigin(unsigned origin, bool fallback=false):previous(xu4_draw_origin_get()){if(!fallback||!previous)xu4_draw_origin_set(origin);}
 ~Xu4DrawOrigin(){xu4_draw_origin_set(previous);}
};
class Xu4DrawTrace {
 Xu4DrawState state; unsigned origin; uint64_t mark,parts[5]{};
public:
 Xu4DrawTrace(uintptr_t map,int x,int y,int z,int view,int cycle,unsigned flags):state{map,x,y,z,view,cycle,flags,2166136261u,false},origin(xu4_draw_origin_get()),mark(time_us_64()){}
 void software(){state.software=true;}
 void tile(uint32_t value){state.tiles=(state.tiles^value)*16777619u;}
 void phase(unsigned p){uint64_t now=time_us_64();parts[p]+=now-mark;mark=now;}
 ~Xu4DrawTrace(){phase(4);xu4_draw_complete(state,origin,parts);}
};
#define XU4_DRAW_ORIGIN(id) Xu4DrawOrigin draw_origin(id)
#define XU4_DRAW_CALLBACK() Xu4DrawOrigin draw_origin(4,true)
#else
#define XU4_DRAW_ORIGIN(id) ((void)0)
#define XU4_DRAW_CALLBACK() ((void)0)
#endif
