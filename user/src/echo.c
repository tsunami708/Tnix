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


// echo [path]
void
main(const char* option)
{
  if (! option || *option == '\0' || ! checkop(option)) {
    write(STDOUT, "echo fail\n", 10);
    exit(1);
  }
  write(STDOUT, option, strlen(option));
  write(STDOUT, "\n", 1);
  exit(0);
}
