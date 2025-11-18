#include "util/string.h"

void*
memset(void* d, int c, u32 n)
{
  for (char* p = d; n--; ++p)
    *p = c;
  return d;
}

void*
memcpy(void* dst, const void* src, u32 n)
{
  if (n == 0)
    return dst;
  const char* s = src;
  char* d = dst;
  if (s < d && s + n > d) {
    s += n;
    d += n;
    while (n-- > 0)
      *--d = *--s;
  } else
    while (n-- > 0)
      *d++ = *s++;
  return dst;
}

char*
strncpy(char* s, const char* t, int n)
{
  char* os = s;
  while (n-- > 0 && (*s++ = *t++) != 0)
    ;
  while (n-- > 0)
    *s++ = 0;
  return os;
}

int
strlen(const char* str)
{
  int len = 0;
  while (str[len] != '\0')
    len++;
  return len;
}

char*
strcpy(char* dest, const char* src)
{
  char* d = dest;
  while ((*d++ = *src++) != '\0')
    ;
  return dest;
}