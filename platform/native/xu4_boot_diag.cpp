#include "xu4_boot_diag.h"
#include "pico/stdlib.h"
#include "hardware/sync.h"
#include "hardware/uart.h"
#include "xu4_close_diag.h"
#include <cstdio>
// Core0 startup only; IRQ never calls stdio, allocator or a blocking UART API.
static volatile bool active;
static volatile uint32_t stage_id, operation, argument, sequence, changed_at, stage_at;
static volatile uint32_t heap_end, heap_request, heap_failed;
static char filename[80];
static repeating_timer_t timer;
static char line[400];
static unsigned length, sent;
static uint32_t last_report;
static void add(const char *s){while(*s && length<sizeof(line))line[length++]=*s++;}
static void hex(uint32_t x){for(int i=28;i>=0;i-=4){char c="0123456789abcdef"[(x>>i)&15];if(length<sizeof(line))line[length++]=c;}}
static bool tick(repeating_timer_t*) {
    if(!active)return false;
    const uint32_t now=time_us_32();
    if(sent==length && uint32_t(now-last_report)>=2000000){
        length=sent=0;last_report=now;
        add(uint32_t(now-changed_at)>=2000000 ? "\nXU4_BOOT_STALL hex=1 stage=" : "\nXU4_BOOT_HEART hex=1 stage=");hex(stage_id);
        add(" wait=");hex(operation);add(" arg=");hex(argument);
        add(" stage_us=");hex(now-stage_at);add(" age_us=");hex(now-changed_at);add(" seq=");hex(sequence);
        add(" brk=");hex(heap_end);add(" request=");hex(heap_request);add(" oom=");hex(heap_failed);
        add(" close=");hex(xu4_close_state[0]);add(" object=");hex(xu4_close_state[1]);
        add(" file=");add(filename);add("\n");
    }
    // One owner on core0; direct FIFO stores bounded to 16 bytes, never wait.
    for(unsigned i=0;i<16 && sent<length && uart_is_writable(uart_default);++i)
        uart_get_hw(uart_default)->dr=(unsigned char)line[sent++];
    return true;
}
extern "C" void xu4_boot_start(){
    xu4_close_enabled=1;
    active=true;stage_at=changed_at=last_report=time_us_32();
    bool ok=add_repeating_timer_ms(-10,tick,nullptr,&timer);
    printf("XU4_BOOT_DIAG build=bootdiag1 timer=%u stall_ms=2000 scope=until_first_frame wait=0:cpu,1:open,2:read,3:seek,4:stat,5:xml_malloc,6:xml_realloc,7:xml_free,8:close\n",unsigned(ok));
}
extern "C" void xu4_boot_finish(){
    xu4_close_enabled=0;
    active=false;cancel_repeating_timer(&timer);
    puts("\nXU4_BOOT_DIAG done=first_frame");
}
extern "C" void xu4_boot_stage(unsigned id,const char *name){
    if(!active)return;
    uint32_t irq=save_and_disable_interrupts();stage_id=id;++sequence;stage_at=changed_at=time_us_32();restore_interrupts(irq);
    printf("XU4_BOOT_STAGE id=%u name=%s\n",id,name);
}
extern "C" void xu4_boot_file(const char *name){
    if(!active)return;
    uint32_t irq=save_and_disable_interrupts();unsigned i=0;
    for(;name && name[i] && i<sizeof(filename)-1;++i){unsigned char c=name[i];filename[i]=(c<=32||c>=127)?'_':c;}
    filename[i]=0;++sequence;changed_at=time_us_32();restore_interrupts(irq);
    printf("XU4_BOOT_FILE name=%s\n",filename);
}
extern "C" unsigned xu4_boot_wait(unsigned op,unsigned arg){
    if(!active)return 0;
    uint32_t irq=save_and_disable_interrupts();unsigned old=operation;
    operation=op;argument=arg;++sequence;changed_at=time_us_32();restore_interrupts(irq);return old;
}
extern "C" void xu4_boot_unwait(unsigned old){
    if(!active)return;
    uint32_t irq=save_and_disable_interrupts();operation=old;argument=0;++sequence;changed_at=time_us_32();restore_interrupts(irq);
}
extern "C" void xu4_boot_heap(uintptr_t end,int amount,unsigned failed){
    if(!active)return;
    heap_end=end;heap_request=uint32_t(amount);heap_failed=failed;
}


extern "C" void __not_in_flash_func(xu4_boot_fault_snapshot)(uint32_t *out){
 out[0]=stage_id;out[1]=operation;out[2]=argument;out[3]=sequence;
 out[4]=xu4_close_state[0];out[5]=xu4_close_state[1];out[6]=xu4_close_state[2];out[7]=xu4_close_state[3];out[8]=xu4_close_enabled;
}
