#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void xu4_read_begin(int id,const char *path);
void xu4_read_end(int rc);
int xu4_read_active(void);
void xu4_read_record(unsigned requested,unsigned actual,unsigned error,uint64_t elapsed,unsigned position);
void xu4_read_open(unsigned error,uint64_t elapsed,unsigned size);
void xu4_read_seek(unsigned error,uint64_t elapsed,unsigned from,unsigned to);
void xu4_read_report(uint64_t now);
#ifdef __cplusplus
}
#endif
