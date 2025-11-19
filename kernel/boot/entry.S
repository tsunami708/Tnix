.section .text.entry
.global entry
.extern start
.extern cpu_stack

entry:
  li a3, 4096 #为每一个CPU分配4KB大小的初始栈空间
  la sp, cpu_stack
  csrr a1, mhartid
  addi a0, a1, 1
  mul a2, a0, a3 
  add sp, sp, a2
  call start

.spin:
  j .spin  #如果没有问题,这条指令永远不会被执行