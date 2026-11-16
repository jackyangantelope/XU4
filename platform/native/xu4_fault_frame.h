#pragma once
#include <stdint.h>
static inline uintptr_t xu4_fault_frame_address(uintptr_t sp,uint32_t exc,uint32_t cfsr){
 // Secure SRAM only. Reject failed stack/lazy-FP operations before dereferencing.
 if(!(exc&0x40u) || (cfsr&0x3838u) || (sp&3u))return 0;
 uintptr_t offset=(exc&0x10u)?0u:18u*4u;
 if(sp<0x20000000u || sp>0x20082000u-offset-32u)return 0;
 return sp+offset;
}
