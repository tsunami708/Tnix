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

context_switch:
  ld ra, CT_RA(a0)
  ld sp, CT_SP(a0)
  ld s0, CT_S0(a0)
  ld s1, CT_S1(a0)
  ld s2, CT_S2(a0)
  ld s3, CT_S3(a0)
  ld s4, CT_S4(a0)
  ld s5, CT_S5(a0)
  ld s6, CT_S6(a0)
  ld s7, CT_S7(a0)
  ld s8, CT_S8(a0)
  ld s9, CT_S9(a0)
  ld s10, CT_S10(a0)
  ld s11, CT_S11(a0)

  sd ra, CT_RA(a1) #important
  sd sp, CT_SP(a1)
  sd s0, CT_S0(a1)
  sd s1, CT_S1(a1)
  sd s2, CT_S2(a1)
  sd s3, CT_S3(a1)
  sd s4, CT_S4(a1)
  sd s5, CT_S5(a1)
  sd s6, CT_S6(a1)
  sd s7, CT_S7(a1)
  sd s8, CT_S8(a1)
  sd s9, CT_S9(a1)
  sd s10, CT_S10(a1)
  sd s11, CT_S11(a1)

  ret

#每一个task第一次被调度时的入口
.section .text.trampoline
.global first_sched
#define TASK_ENTRY 0

#define TASK_UPAGETABLE 16
#define TASK_POINTER    24
first_sched:
  ld t1, TASK_POINTER(tp)
  ld t1, TASK_ENTRY(t1)
  csrw sepc, t1

  sfence.vma zero, zero
  ld t0, TASK_UPAGETABLE(tp) #切换用户task页表
  csrw satp, t0
  sfence.vma zero, zero

  sret  