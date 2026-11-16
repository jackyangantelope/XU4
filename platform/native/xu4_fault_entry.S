.syntax unified
.cpu cortex-m33
.thumb
.section .time_critical.xu4_fault_entry,"ax",%progbits
.global isr_hardfault
.global isr_memmanage
.global isr_busfault
.global isr_usagefault
.type isr_hardfault,%function
.thumb_func
isr_hardfault:
 b xu4_fault_entry
.thumb_func
isr_memmanage:
 b xu4_fault_entry
.thumb_func
isr_busfault:
 b xu4_fault_entry
.thumb_func
isr_usagefault:
 b xu4_fault_entry
.thumb_func
xu4_fault_entry:
 cpsid i
 tst lr,#4
 ite eq
 mrseq r0,msp
 mrsne r0,psp
 mov r1,lr
 ldr r2,=0xd0000000
 ldr r2,[r2]
 and r2,r2,#1
 mrs r3,ipsr
 ldr r12,=xu4_fault_stacks
 add r12,r12,r2,lsl #11
 add r12,r12,#2048
 msr msp,r12
 b xu4_fault_capture
.ltorg
