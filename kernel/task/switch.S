#define CT_RA  0
#define CT_SP  8
#define CT_S0  16
#define CT_S1  24
#define CT_S2  32
#define CT_S3  40
#define CT_S4  48
#define CT_S5  56
#define CT_S6  64
#define CT_S7  72
#define CT_S8  80
#define CT_S9  88
#define CT_S10 96
#define CT_S11 104

.section .text
.global context_switch
.global dump_context

.macro store_context
  sd ra, CT_RA(a0)
  sd sp, CT_SP(a0)
  sd s0, CT_S0(a0)
  sd s1, CT_S1(a0)
  sd s2, CT_S2(a0)
  sd s3, CT_S3(a0)
  sd s4, CT_S4(a0)
  sd s5, CT_S5(a0)
  sd s6, CT_S6(a0)
  sd s7, CT_S7(a0)
  sd s8, CT_S8(a0)
  sd s9, CT_S9(a0)
  sd s10, CT_S10(a0)
  sd s11, CT_S11(a0)
.endm

dump_context:
  store_context
  ret

context_switch:
  beqz a0, .L
  store_context  
.L:
  ld ra, CT_RA(a1) #important
  ld sp, CT_SP(a1)
  ld s0, CT_S0(a1)
  ld s1, CT_S1(a1)
  ld s2, CT_S2(a1)
  ld s3, CT_S3(a1)
  ld s4, CT_S4(a1)
  ld s5, CT_S5(a1)
  ld s6, CT_S6(a1)
  ld s7, CT_S7(a1)
  ld s8, CT_S8(a1)
  ld s9, CT_S9(a1)
  ld s10, CT_S10(a1)
  ld s11, CT_S11(a1)

  ret

.section .text.trampoline
.global run_new_task
run_new_task:
  sfence.vma zero, zero
  csrw satp, a0  #切换用户task页表
  sfence.vma zero, zero
  mv sp, a1
  mv a0, sp
  sret