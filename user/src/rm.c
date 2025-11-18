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

// rm [path] 只支持一个参数
void
main(const char* option)
{
  if (! option || *option == '\0' || ! checkop(option)) {
    write(STDOUT, "rm fail\n", 8);
    exit(1);
  }
  if (unlink(option)) {
    write(STDOUT, "rm fail\n", 8);
    exit(1);
  }
  exit(0);
}