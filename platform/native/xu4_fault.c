#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "xu4_close_diag.h"
#include "xu4_fault_frame.h"
__attribute__((aligned(8))) unsigned char xu4_fault_stacks[2][2048];
struct Xu4FaultRecord {uint32_t magic,core,ipsr,exc,sp,cfsr,hfsr,mmfar,bfar,valid,r[8],last[9];};
volatile struct Xu4FaultRecord xu4_fault_records[2];
static unsigned uart_budget;
// Writable arrays ensure all exception strings are resident in SRAM.
static char prefix[]="\nXU4_FAULT hex=1";
static char keys[][12]={" core="," ipsr="," exc="," sp="," cfsr="," hfsr="," mmfar="," bfar="," valid="," r0="," r1="," r2="," r3="," r12="," lr="," pc="," xpsr="," stage="," wait="," arg="," seq="," close="," object="," close_seq="," caller="," enabled="};
static void __not_in_flash_func(raw_char)(char c){
 while(uart_budget){--uart_budget;if(uart_is_writable(uart_default)){uart_get_hw(uart_default)->dr=(unsigned char)c;return;}}
}
static void __not_in_flash_func(raw_text)(const char *s){while(*s)raw_char(*s++);}
static void __not_in_flash_func(raw_hex)(uint32_t v){for(int shift=28;shift>=0;shift-=4){unsigned n=(v>>shift)&15;raw_char(n<10?'0'+n:'a'+n-10);}}
__attribute__((noreturn)) void __not_in_flash_func(xu4_fault_capture)(uint32_t *sp,uint32_t exc,uint32_t core,uint32_t ipsr){
 volatile struct Xu4FaultRecord *r=&xu4_fault_records[core&1];r->magic=0x58553446;r->core=core;r->ipsr=ipsr;r->exc=exc;r->sp=(uint32_t)(uintptr_t)sp;
 r->cfsr=*(volatile uint32_t*)0xe000ed28u;r->hfsr=*(volatile uint32_t*)0xe000ed2cu;
 r->mmfar=*(volatile uint32_t*)0xe000ed34u;r->bfar=*(volatile uint32_t*)0xe000ed38u;
 uintptr_t frame=xu4_fault_frame_address((uintptr_t)sp,exc,r->cfsr);r->valid=frame!=0;
 for(unsigned i=0;i<8;++i)r->r[i]=frame?((volatile uint32_t*)frame)[i]:0;
 uint32_t last[9];xu4_boot_fault_snapshot(last);for(unsigned i=0;i<9;++i)r->last[i]=last[i];
 uart_budget=5000000;raw_text(prefix);
 uint32_t fields[26];fields[0]=r->core;fields[1]=r->ipsr;fields[2]=r->exc;fields[3]=r->sp;fields[4]=r->cfsr;fields[5]=r->hfsr;fields[6]=r->mmfar;fields[7]=r->bfar;fields[8]=r->valid;
 for(unsigned i=0;i<8;++i)fields[9+i]=r->r[i];for(unsigned i=0;i<9;++i)fields[17+i]=r->last[i];
 for(unsigned i=0;i<26;++i){raw_text(keys[i]);raw_hex(fields[i]);}raw_char('\n');
 for(;;)__asm volatile("wfe");
}

