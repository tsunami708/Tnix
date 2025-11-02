#define AS
#include "kernel/syscall/syscall.h"

.global fork
fork:
  li a7, SYS_FORK
  ecall
  ret

.global exit
exit:
  li a7, SYS_EXIT
  ecall
  ret

.global exec
exec:
  li a7, SYS_EXEC
  ecall
  ret

.global wait
wait:
  li a7, SYS_WAIT
  ecall
  ret