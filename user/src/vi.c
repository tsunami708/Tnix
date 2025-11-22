#include "usys.h"

bool
checkop(const char* option)
{
  while (*option) {
    if (*option == ' ')
      return false;
    ++option;
  }
  return true;
}

void
main(const char* ofile)
{
  if (! ofile || *ofile == '\0' || ! checkop(ofile)) {
    write(STDOUT, "vi fail\n", 8);
    exit(1);
  }
  int fd = open(ofile, O_RDONLY | O_CREAT);
  int n;
  static char buf[32] = { 0 };
  while ((n = read(fd, buf, sizeof(buf))))
    write(STDOUT, buf, n);

  close(fd);
  fd = open(ofile, O_RDWR | O_APPEND | O_CREAT);
  while (1) {
    read(STDIN, buf, 1);
    if (buf[0] == '\x04')
      break;
    write(fd, buf, 1);
  }
  write(STDOUT, "\n", 1);
  close(fd);
  exit(0);
}