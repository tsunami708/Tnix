#include "usys.h"

void
main(int, char*[])
{
  int pid = fork();
  if (pid == 0) {
    exec("/bin/sh", 0);
    exit(1);
  } else if (pid > 0) {
    int status;
    wait(&status);
  }
  exit(0);
}