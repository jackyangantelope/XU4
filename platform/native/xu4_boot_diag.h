#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void xu4_boot_start(void);
void xu4_boot_finish(void);
void xu4_boot_stage(unsigned stage, const char *name);
void xu4_boot_file(const char *name);
unsigned xu4_boot_wait(unsigned op, unsigned arg);
void xu4_boot_unwait(unsigned previous);
void xu4_boot_heap(uintptr_t end, int amount, unsigned failed);
#ifdef __cplusplus
}
#endif
