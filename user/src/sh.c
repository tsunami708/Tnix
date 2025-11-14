#include "usys.h"

void
main(void)
{
  int fd1 = open("/dev/console", O_RDONLY);
  int fd2 = open("/dev/console", O_WRONLY);
  (void)fd2;
  int pid, status;
  while (1) {

    static char cmd[128] = { 0 };
    int n = read(fd1, cmd, 127);
    cmd[n] = '\0';

    pid = fork();
    if (pid == 0) {
      int m = 0;
      for (; cmd[m] != ' ' && cmd[m] != '\0'; ++m)
        ;
      if (cmd[m] == ' ') {
        cmd[m] = 0;
        exec(cmd, cmd + m + 1);
      } else
        exec(cmd, NULL);
      exit(1);
    }

    wait(&status);
  }
}