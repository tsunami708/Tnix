#define AS
#include "kernel/syscall/syscall.h"

.section .init.text
.global fork1
fork1:
  li a7, SYS_FORK
  ecall
  ret

.global exit1
exit1:
  li a7, SYS_EXIT
  ecall
  ret

.global exec1
exec1:
  li a7, SYS_EXEC
  ecall
  ret

.global wait1
wait1:
  li a7, SYS_WAIT
  ecall
  ret