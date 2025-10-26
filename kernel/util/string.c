#include "util/string.h"

void*
memset1(void* dst, int c, u32 n)
{
  char* cdst = (char*)dst;
  int i;
  for (i = 0; i < n; i++) {
    cdst[i] = c;
  }
  return dst;
}

static void*
memmove(void* dst, const void* src, u32 n)
{
  const char* s;
  char* d;
  if (n == 0)
    return dst;

  s = src;
  d = dst;
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

void*
memcpy1(void* dst, const void* src, u32 n)
{
  return memmove(dst, src, n);
}

int
strncmp1(const char* p, const char* q, u32 n)
{
  while (n > 0 && *p && *p == *q)
    n--, p++, q++;
  if (n == 0)
    return 0;
  return (u8)*p - (u8)*q;
}

char*
strncpy1(char* s, const char* t, int n)
{
  char* os;
  os = s;
  while (n-- > 0 && (*s++ = *t++) != 0)
    ;
  while (n-- > 0)
    *s++ = 0;
  return os;
}


int
strlen1(const char* s)
{
  int n;
  for (n = 0; s[n]; n++)
    ;
  return n;
}
