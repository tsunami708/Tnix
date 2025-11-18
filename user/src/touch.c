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

// touch [path] 只支持一个参数
void
main(const char* option)
{
  if (! option || *option == '\0' || ! checkop(option)) {
    write(STDOUT, "touch fail\n", 11);
    exit(1);
  }
  if (open(option, O_CREAT | O_RDONLY) < 0) {
    write(STDOUT, "touch fail\n", 11);
    exit(1);
  }
  exit(0);
}