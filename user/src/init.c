#include "usys.h"

void
main(void)
{
  mknod("/dev/console", 1);
  int pid = fork();
  if (pid == 0) {
    exec("/bin/sh", NULL);
    exit(1);
  }
  while (1)
    wait(NULL);
}
