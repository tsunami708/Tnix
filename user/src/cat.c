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

static char buf[64];
// cat [path]
void
main(const char* option)
{
  if (! option || *option == '\0' || ! checkop(option)) {
    write(STDOUT, "cat fail\n", 9);
    exit(1);
  }
  int fd = open(option, O_RDONLY);
  if (fd < 0) {
    write(STDOUT, "cat fail\n", 9);
    exit(1);
  }
  while (1) {
    int n = read(fd, buf, sizeof(buf));
    if (n == 0)
      break;
    write(STDOUT, buf, n);
  }

  exit(0);
}
