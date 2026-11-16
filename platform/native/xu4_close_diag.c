#include "xu4_close_diag.h"
#include "pico/stdlib.h"
#include <stdio.h>
volatile uint32_t xu4_close_enabled;
volatile uint32_t xu4_close_state[4];
void xu4_close_mark(unsigned step,uintptr_t object,int emit){
 if(!xu4_close_enabled)return;
 xu4_close_state[0]=step;xu4_close_state[1]=(uint32_t)object;++xu4_close_state[2];
 xu4_close_state[3]=(uint32_t)(uintptr_t)__builtin_return_address(0);
 if(emit)printf("XU4_CLOSE step=%u object=%08lx seq=%lu\n",step,(unsigned long)object,(unsigned long)xu4_close_state[2]);
}

