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

.global read
read:
  li a7, SYS_READ
  ecall
  ret

.global write
write:
  li a7, SYS_WRITE
  ecall
  ret

.global lseek
lseek:
  li a7, SYS_LSEEK
  ecall
  ret

.global open
open:
  li a7, SYS_OPEN
  ecall
  ret

.global dup
dup:
  li a7, SYS_DUP
  ecall
  ret

.global close
close:
  li a7, SYS_CLOSE
  ecall
  ret

.global link
link:
  li a7, SYS_LINK
  ecall
  ret

.global unlink
unlink:
  li a7, SYS_UNLINK
  ecall
  ret

.global mkdir
mkdir:
  li a7, SYS_MKDIR
  ecall
  ret

.global rmdir
rmdir:
  li a7, SYS_RMDIR
  ecall
  ret

.global mknod
mknod:
  li a7, SYS_MKNOD
  ecall
  ret

.global chdir
chdir:
  li a7, SYS_CHDIR
  ecall
  ret

.global alloc
alloc:
  li a7, SYS_ALLOC
  ecall
  ret

.global free
free:
  li a7, SYS_FREE
  ecall
  ret

.global pipe
pipe:
  li a7, SYS_PIPE
  ecall
  ret

.global ls
ls:
  li a7, SYS_LS
  ecall
  ret

.global sleep
sleep:
  li a7, SYS_SLEEP
  ecall
  ret