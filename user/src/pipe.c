#include "usys.h"

void
main(void)
{
  int fd[2];
  pipe(fd);
  int pid = fork();
  if (pid == 0) {
    close(fd[1]);
    char ch;
    for (;;) {
      read(fd[0], &ch, 1);
      write(STDOUT, &ch, 1);
    }
    // while (1)
    //   ;
    close(fd[0]);
    exit(0);
  }
  close(fd[0]);
  for (int i = 0; i < 10; ++i)
    write(fd[1], "p", 1);
  close(fd[1]);
  wait(NULL);
  exit(0);
}