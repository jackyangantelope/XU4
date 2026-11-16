#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
extern volatile uint32_t xu4_close_enabled;
extern volatile uint32_t xu4_close_state[4];
void xu4_close_mark(unsigned step,uintptr_t object,int emit);
void xu4_boot_fault_snapshot(uint32_t *out);
#ifdef __cplusplus
}
#endif
