#include "usys.h"
#include "str.h"

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

// rmdir [path] 只支持一个参数
void
main(const char* option)
{
  if (! option || *option == '\0' || ! checkop(option)) {
    write(STDOUT, "rmdir fail\n", 11);
    exit(1);
  }
  if (strcmp(option, ".") == 0 || strcmp(option, "..") == 0 || rmdir(option)) {
    write(STDOUT, "rmdir fail\n", 11);
    exit(1);
  }
  exit(0);
}