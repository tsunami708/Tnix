#include "usys.h"

void
main(void)
{
  int fd = open("/dev/console", O_RDWR);
  if (fd < 0) {
    mknod("/dev/console", 1);
    fd = open("/dev/console", O_RDWR);
  }

  // int pid = fork();
  // if (pid == 0) {
  //   exec("/bin/sh", 0);
  //   exit(1);
  // } else if (pid > 0) {
  //   int status;
  //   wait(&status);
  // }
  mkdir("tmp");
  chdir("tmp");
  chdir("..");
  rmdir("tmp");

  close(fd);
  exit(0);
}