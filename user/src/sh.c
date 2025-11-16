#include "usys.h"

static char cmd[128];

void
main(void)
{
  open("/dev/console", O_RDONLY);
  open("/dev/console", O_WRONLY);

  int pid, status;
  while (1) {
    write(STDOUT, "$>", 2);
    unsigned int n = read(STDIN, cmd, sizeof(cmd) - 1);
    cmd[n] = 0;
    if (*cmd == '\r' || *cmd == '\n') {
      ;
    } else if (*cmd == 'c' && *(cmd + 1) == 'd' && *(cmd + 2) == ' ') {
      cmd[n - 1] = 0;
      chdir(cmd + 3);
    } else {
      cmd[n - 1] = 0;
      pid = fork();
      if (pid == 0) {
        exec(cmd, NULL);
        exit(1);
      }
      wait(&status);
    }
  }
}