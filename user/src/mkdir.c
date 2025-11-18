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

// mkdir [path] 只支持一个参数
void
main(const char* option)
{
  if (! option || *option == '\0' || ! checkop(option)) {
    write(STDOUT, "mkdir fail\n", 11);
    exit(1);
  }
  if (mkdir(option)) {
    write(STDOUT, "mkdir fail\n", 11);
    exit(1);
  }
  exit(0);
}