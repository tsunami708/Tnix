#include "usys.h"

void
main(void)
{
  // int fd = open("/dev/console", O_RDWR);
  // if (fd < 0) {
  //   mknod("/dev/console", 1);
  //   fd = open("/dev/console", O_RDWR);
  // }

  // write(fd, "Hello World", 11);

  // int fd1 = dup(fd);
  // int fd2 = dup(fd);
  // close(fd);
  // close(fd1);
  // close(fd2);

  // int pid = fork();
  // if (pid == 0) {
  //   exec("/bin/sh", 0);
  //   exit(1);
  // } else if (pid > 0) {
  //   int status;
  //   wait(&status);
  // }
  int fd = open("file", O_RDWR | O_CREAT);
  link("file", "file1");
  link("file1", "file2");

  write(fd, "Hello World", 11);
  char buf[12] = { 0 };
  lseek(fd, -11);
  read(fd, buf, 11);

  close(fd);
  unlink("file1");
  unlink("file");
  unlink("file2");

  mkdir("tmp");
  chdir("tmp");
  chdir("..");
  rmdir("tmp");

  exit(0);
}
