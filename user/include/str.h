#pragma once

static inline __attribute__((always_inline)) int
strcmp(const char* s1, const char* s2)
{
  while (*s1 && *s2 && *s1 == *s2) {
    ++s1;
    ++s2;
  }
  return *s1 != *s2;
}
static inline __attribute__((always_inline)) int
strlen(const char* str)
{
  int len = 0;
  while (str[len] != '\0')
    len++;
  return len;
}
static inline __attribute__((always_inline)) char*
strcpy(char* dest, const char* src)
{
  char* d = dest;
  while ((*d++ = *src++) != '\0')
    ;
  return dest;
}