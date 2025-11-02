#include "usys.h"

//* init
void
main(int, char*[])
{
  int pid = fork();
  if (pid == 0) {
    // exec("/bin/sh", 0);
    exit(0);
  } else if (pid > 0) {
    wait(NULL);
  }
  exit(0);
}