#include "usys.h"
#include "str.h"

void
main(const char* option)
{
  write(STDOUT, option, strlen(option));
  exit(0);
}