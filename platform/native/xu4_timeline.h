#pragma once
#include <stdint.h>
void xu4_timeline_begin(unsigned category,uint64_t now);
void xu4_timeline_end(unsigned category,uint64_t now);
void xu4_timeline_frame(uint64_t now);
void xu4_timeline_window(uint64_t now);
void xu4_timeline_drain(); // one bounded line, only with low UART backlog
void xu4_timeline_block(uint64_t start,uint64_t end,unsigned q0,unsigned q1,int64_t video_left,int64_t wait_left);
unsigned xu4_timeline_context();
extern "C" bool xu4_uart_trace_room();
