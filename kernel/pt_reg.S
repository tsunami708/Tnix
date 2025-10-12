#define PT_RA      0
#define PT_SP      8
#define PT_GP      16
#define PT_TP      24
#define PT_SEPC    32
#define PT_SSTATUS 40
#define PT_STVAL   48
#define PT_SCAUSE  56
#define PT_T0      64
#define PT_T1      72
#define PT_T2      80
#define PT_T3      88
#define PT_T4      96
#define PT_T5      104
#define PT_T6      112
#define PT_S0      120
#define PT_S1      128
#define PT_S2      136
#define PT_S3      144
#define PT_S4      152
#define PT_S5      160
#define PT_S6      168
#define PT_S7      176
#define PT_S8      184
#define PT_S9      192
#define PT_S10     200
#define PT_S11     208
#define PT_A0      216
#define PT_A1      224
#define PT_A2      232
#define PT_A3      240
#define PT_A4      248
#define PT_A5      256
#define PT_A6      264
#define PT_A7      272
#define PT_SIZE    280

.macro save_trap_context
  addi sp, sp, -PT_SIZE
  sd ra, PT_RA(sp)
  sd gp, PT_GP(sp)

  sd tp, PT_TP(sp)
  sd t0, PT_T0(sp)
  sd t1, PT_T1(sp)
  sd t2, PT_T2(sp)
  sd t3, PT_T3(sp)
  sd t4, PT_T4(sp)
  sd t5, PT_T5(sp)
  sd t6, PT_T6(sp)

  sd s0, PT_S0(sp)
  sd s1, PT_S1(sp)
  sd s2, PT_S2(sp)
  sd s3, PT_S3(sp)
  sd s4, PT_S4(sp)
  sd s5, PT_S5(sp)
  sd s6, PT_S6(sp)
  sd s7, PT_S7(sp)
  sd s8, PT_S8(sp)
  sd s9, PT_S9(sp)
  sd s10, PT_S10(sp)
  sd s11, PT_S11(sp)

  sd a0, PT_A0(sp)
  sd a1, PT_A1(sp)
  sd a2, PT_A2(sp)
  sd a3, PT_A3(sp)
  sd a4, PT_A4(sp)
  sd a5, PT_A5(sp)
  sd a6, PT_A6(sp)
  sd a7, PT_A7(sp)

  csrr a0, sepc
  sd a0,PT_SEPC(sp)
  csrr a0, sstatus
  sd a0,PT_SSTATUS(sp)
  csrr a0, stval
  sd a0,PT_STVAL(sp)
  csrr a0, scause
  sd a0,PT_SCAUSE(sp)
.endm

.macro recover_trap_context
  ld a0, PT_A0(sp)
  ld a1, PT_A1(sp)
  ld a2, PT_A2(sp)
  ld a3, PT_A3(sp)
  ld a4, PT_A4(sp)
  ld a5, PT_A5(sp)
  ld a6, PT_A6(sp)
  ld a7, PT_A7(sp)

  ld s0, PT_S0(sp)
  ld s1, PT_S1(sp)
  ld s2, PT_S2(sp)
  ld s3, PT_S3(sp)
  ld s4, PT_S4(sp)
  ld s5, PT_S5(sp)
  ld s6, PT_S6(sp)
  ld s7, PT_S7(sp)
  ld s8, PT_S8(sp)
  ld s9, PT_S9(sp)
  ld s10, PT_S10(sp)
  ld s11, PT_S11(sp)

  ld t0, PT_T0(sp)
  ld t1, PT_T1(sp)
  ld t2, PT_T2(sp)
  ld t3, PT_T3(sp)
  ld t4, PT_T4(sp)
  ld t5, PT_T5(sp)
  ld t6, PT_T6(sp)


  ld gp, PT_GP(sp)
  ld ra, PT_RA(sp)

  addi sp, sp, PT_SIZE
.endm

/*stvec的值,直接跳转模式trap_entry地址必须4字节对齐*/
.global ktrap_entry
.global trampoline
.global utrap_entry
.extern do_trap

.section .text
.align 2
ktrap_entry:
  save_trap_context
  addi a0, sp, PT_SIZE
  sd a0, PT_SP(sp)
  mv a0, sp
  call do_trap
  recover_trap_context
  sret


.section .text.trampoline
#include "config.h"
#define TASK_KSTACK 8
#define TASK_USATP  16
#define DO_TRAP 32
.align 2
utrap_entry:
  csrw sscratch, sp #保存用户态栈指针
  
  sfence.vma zero, zero
  li sp, TRAPFRAME
  ld sp, 0(sp) #获取内核页表
  csrw satp, sp
  sfence.vma zero, zero

  ld sp, TASK_KSTACK(tp) #获取内核栈地址
  save_trap_context
  csrr a0, sscratch
  sd a0, PT_SP(sp)

  mv a0, sp
  ld t0, DO_TRAP(tp)
  jalr t0     #不适用call指令,call是相对寻址,再高虚拟地址执行会出现页表访问错误

  ld a0, PT_SP(sp)
  csrw sscratch, a0
  recover_trap_context

  sfence.vma zero, zero
  ld sp, TASK_USATP(tp)
  csrw satp, sp
  sfence.vma zero, zero
  csrr sp, sscratch  #恢复用户栈
  sret



